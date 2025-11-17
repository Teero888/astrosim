#include "proceduralmesh.h"
#include "camera.h"
#include "generated/embedded_shaders.h"
#include "glm/geometric.hpp"
#include "marchingcubes.h"
#include <algorithm>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <map>

static glm::vec3 InterpolateVertex(glm::vec3 p1, glm::vec3 p2, float d1, float d2)
{
	if(glm::abs(d1 - d2) < 0.00001f)
	{
		return p1;
	}
	float t = (0.0f - d1) / (d2 - d1);
	return glm::mix(p1, p2, t);
}

CProceduralMesh::CProceduralMesh()
{
	m_bRunWorker = true;
}

CProceduralMesh::~CProceduralMesh()
{
	Destroy();
}

void CProceduralMesh::Init(SBody *pBody, EBodyType bodyType, int voxelResolution)
{
	m_pBody = pBody;
	m_BodyType = bodyType;

	// Compile shader for all renderable types
	m_Shader.CompileShader(Shaders::VERT_BODY, Shaders::FRAG_BODY);

	// Only initialize terrain generator for terrestrial bodies
	if(m_BodyType == EBodyType::TERRESTRIAL)
		m_TerrainGenerator.Init(pBody->m_Id, pBody->m_RenderParams.m_Terrain, pBody->m_RenderParams.m_TerrainType);

	// Create an octree root for all renderable types (Stars, Planets)
	// For Stars, the terrain generator will just use default noise,
	// but the shader will override the color to be emissive.
	// TODO: Add GAS_GIANT when supported
	if(m_BodyType == EBodyType::TERRESTRIAL || m_BodyType == EBodyType::STAR)
	{
		float RootSize = (float)m_pBody->m_RenderParams.m_Radius * 2.0f;
		m_pRootNode = std::make_shared<COctreeNode>(this, std::weak_ptr<COctreeNode>(), glm::vec3(0.0f), RootSize, 0, voxelResolution);
	}

	unsigned int NumThreads = std::thread::hardware_concurrency();
	// Use all but one core for workers, leave one for main thread/etc.
	NumThreads = (NumThreads > 1) ? NumThreads - 1 : 1;

	for(unsigned int i = 0; i < NumThreads; ++i)
		m_vWorkerThreads.emplace_back(&CProceduralMesh::GenerationWorkerLoop, this);
}

void CProceduralMesh::Update(CCamera &Camera)
{
	CheckApplyQueue();

	if(m_pRootNode)
		m_pRootNode->Update(Camera);
}

void CProceduralMesh::Render(const CCamera &Camera, const SBody *pLightBody)
{
	// Render if we have a root node. (i.e., we are a renderable type)
	if(!m_pRootNode)
		return;

	m_Shader.Use();

	// Pass constant needed for logarithmic depth calculation
	float F = 2.0f / log2(10000.0f + 1.0f); // far plane is 10000.0f
	m_Shader.SetFloat("u_logDepthF", F);

	m_Shader.SetBool("uSource", m_pBody == pLightBody);

	m_Shader.SetMat4("uView", Camera.m_View);
	m_Shader.SetMat4("uProjection", Camera.m_Projection);

	Vec3 LightDir = (pLightBody->m_SimParams.m_Position - m_pBody->m_SimParams.m_Position).normalize();
	m_Shader.SetVec3("uLightDir", (glm::vec3)LightDir);
	m_Shader.SetVec3("uLightColor", pLightBody->m_RenderParams.m_Color);
	m_Shader.SetVec3("uObjectColor", m_pBody->m_RenderParams.m_Color); // This is the base color for non-terrestrial or stars

	m_Shader.SetInt("uTerrainType", (int)m_pBody->m_RenderParams.m_TerrainType);

	// Set color palette uniforms
	m_Shader.SetVec3("DEEP_OCEAN_COLOR", m_pBody->m_RenderParams.m_Colors.deepOcean);
	m_Shader.SetVec3("SHALLOW_OCEAN_COLOR", m_pBody->m_RenderParams.m_Colors.shallowOcean);
	m_Shader.SetVec3("BEACH_COLOR", m_pBody->m_RenderParams.m_Colors.beach);
	m_Shader.SetVec3("LAND_LOW_COLOR", m_pBody->m_RenderParams.m_Colors.landLow);
	m_Shader.SetVec3("LAND_HIGH_COLOR", m_pBody->m_RenderParams.m_Colors.landHigh);
	m_Shader.SetVec3("MOUNTAIN_LOW_COLOR", m_pBody->m_RenderParams.m_Colors.mountainLow);
	m_Shader.SetVec3("MOUNTAIN_HIGH_COLOR", m_pBody->m_RenderParams.m_Colors.mountainHigh);
	m_Shader.SetVec3("SNOW_COLOR", m_pBody->m_RenderParams.m_Colors.snow);

	m_Shader.SetFloat("uPlanetRadius", (float)m_pBody->m_RenderParams.m_Radius);

	// Render the single root node
	if(m_pRootNode)
		m_pRootNode->Render(m_Shader, Camera.m_AbsolutePosition, m_pBody->m_SimParams.m_Position);
}

void CProceduralMesh::Destroy()
{
	m_bRunWorker = false;
	m_GenQueueCV.notify_all(); // Wake up all waiting workers
	m_ApplyQueueCV.notify_all(); // Wake up all waiting workers

	for(auto &thread : m_vWorkerThreads) // Join all threads
	{
		if(thread.joinable())
			thread.join();
	}

	m_pRootNode = nullptr;
	m_Shader.Destroy();
}

void CProceduralMesh::AddToGenerationQueue(std::shared_ptr<COctreeNode> pNode)
{
	{
		std::lock_guard<std::mutex> lock(m_GenQueueMutex);
		m_GenerationQueue.push(pNode);
	}
	m_GenQueueCV.notify_one(); // Notify one waiting worker
}

void CProceduralMesh::CheckApplyQueue()
{
	std::shared_ptr<COctreeNode> pNode = nullptr;
	while(true)
	{
		std::lock_guard<std::mutex> lock(m_ApplyQueueMutex);
		if(m_ApplyQueue.empty())
			break;
		pNode = m_ApplyQueue.front();
		m_ApplyQueue.pop();

		if(pNode)
		{
			pNode->ApplyMeshBuffers();
			m_ApplyQueueCV.notify_one(); // Notify one waiting worker that space is available
		}
	}
}

void CProceduralMesh::GenerationWorkerLoop()
{
	while(m_bRunWorker)
	{
		std::shared_ptr<COctreeNode> pNode = nullptr;

		{
			std::unique_lock<std::mutex> lock(m_GenQueueMutex);
			// Wait until queue is not empty OR worker is told to stop
			m_GenQueueCV.wait(lock, [this] {
				return !m_GenerationQueue.empty() || !m_bRunWorker;
			});

			if(!m_bRunWorker)
				break; // Exit loop if worker is stopping

			if(!m_GenerationQueue.empty())
			{
				pNode = m_GenerationQueue.front();
				m_GenerationQueue.pop();
			}
		}

		if(pNode)
		{
			pNode->GenerateMesh();

			{
				std::unique_lock<std::mutex> lock(m_ApplyQueueMutex);

				// Wait until the apply queue has space
				m_ApplyQueueCV.wait(lock, [this] {
					return m_ApplyQueue.size() < m_MaxApplyQueueSize || !m_bRunWorker;
				});

				if(!m_bRunWorker)
					break;

				m_ApplyQueue.push(pNode);
			}
		}
	}
}

COctreeNode::COctreeNode(CProceduralMesh *pOwnerMesh, std::weak_ptr<COctreeNode> pParent, glm::vec3 center, float size, int level, int voxelResolution) :
	m_pOwnerMesh(pOwnerMesh),
	m_pParent(pParent),
	m_Level(level),
	m_VoxelResolution(voxelResolution),
	m_Center(center),
	m_Size(size),
	m_bIsGenerating(false),
	m_bHasGeneratedData(false),
	m_bGenerationAttempted(false)
{
	// All children are null by default
}

COctreeNode::~COctreeNode()
{
	if(m_VAO != 0)
	{
		glDeleteBuffers(1, &m_EBO);
		glDeleteBuffers(1, &m_VBO);
		glDeleteVertexArrays(1, &m_VAO);
		m_VAO = 0;
		m_VBO = 0;
		m_EBO = 0;
		m_NumIndices = 0;
	}
}

void COctreeNode::GenerateMesh()
{
	// printf("Generating mesh for node level %d\n", m_Level);
	const int res = m_VoxelResolution;
	const int numGridPoints = (res + 1) * (res + 1) * (res + 1);
	const int res1 = res + 1;

	std::vector<float> vDensityGrid(numGridPoints);
	std::vector<glm::vec3> vGradientGrid(numGridPoints);

	float StepSize = m_Size / res;
	glm::vec3 StartCorner = m_Center - glm::vec3(m_Size * 0.5f); // Min corner of the cube

	for(int z = 0; z <= res; ++z)
	{
		for(int y = 0; y <= res; ++y)
		{
			for(int x = 0; x <= res; ++x)
			{
				// Calculate the world position in the planar grid
				glm::vec3 world_pos = StartCorner + glm::vec3(x * StepSize, y * StepSize, z * StepSize);

				int idx = x + y * res1 + z * res1 * res1;
				// Get density from the planar world position
				float radius = (float)m_pOwnerMesh->m_pBody->m_RenderParams.m_Radius;
				vDensityGrid[idx] = m_pOwnerMesh->m_TerrainGenerator.GetTerrainDensity(world_pos, radius);
				vGradientGrid[idx] = m_pOwnerMesh->m_TerrainGenerator.CalculateDensityGradient(world_pos, radius);
			}
		}
	}

	m_vGeneratedVertices.clear();
	m_vGeneratedIndices.clear();
	std::map<std::pair<int, int>, unsigned int> mVertexMap;

	const int aaCornerOffsets[8][3] = {
		{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
		{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};

	const int aaEdgeToCorners[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0},
		{4, 5}, {5, 6}, {6, 7}, {7, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}};

	for(int z = 0; z < res; ++z)
	{
		for(int y = 0; y < res; ++y)
		{
			for(int x = 0; x < res; ++x)
			{
				glm::vec3 Corners[8];
				float Densities[8];
				glm::vec3 Gradients[8];
				int CornerGlobalIndices[8];
				int CubeIndex = 0;

				for(int i = 0; i < 8; ++i)
				{
					int dx = aaCornerOffsets[i][0];
					int dy = aaCornerOffsets[i][1];
					int dz = aaCornerOffsets[i][2];
					int idx = (x + dx) + (y + dy) * res1 + (z + dz) * res1 * res1;

					Corners[i] = StartCorner + glm::vec3((x + dx) * StepSize, (y + dy) * StepSize, (z + dz) * StepSize);
					Densities[i] = vDensityGrid[idx];
					Gradients[i] = vGradientGrid[idx];
					CornerGlobalIndices[i] = idx;
					if(Densities[i] > 0)
						CubeIndex |= (1 << i);
				}

				if(CubeIndex == 0 || CubeIndex == 255)
					continue;

				int edges = MarchingCubesData::ms_EdgeTable[CubeIndex];
				unsigned int aEdgeVertexIndices[12];

				for(int i = 0; i < 12; ++i)
				{
					if(edges & (1 << i))
					{
						int c1_local = aaEdgeToCorners[i][0];
						int c2_local = aaEdgeToCorners[i][1];
						int c1_global = CornerGlobalIndices[c1_local];
						int c2_global = CornerGlobalIndices[c2_local];

						std::pair<int, int> key = (c1_global < c2_global) ?
										  std::make_pair(c1_global, c2_global) :
										  std::make_pair(c2_global, c1_global);

						if(mVertexMap.find(key) == mVertexMap.end())
						{
							glm::vec3 p1 = Corners[c1_local];
							glm::vec3 p2 = Corners[c2_local];
							float d1 = Densities[c1_local];
							float d2 = Densities[c2_local];
							glm::vec3 pos = InterpolateVertex(p1, p2, d1, d2);

							glm::vec3 g1 = Gradients[c1_local];
							glm::vec3 g2 = Gradients[c2_local];
							float t = (0.0f - d1) / (d2 - d1);
							glm::vec3 norm = glm::normalize(glm::mix(g1, g2, t));

							SProceduralVertex vert;
							vert.position = pos - m_Center; // Store position relative to the node's center
							vert.normal = norm;
							vert.texCoord = glm::vec2(pos.x, pos.z) / 1000.0f;
							STerrainColorData colorData = m_pOwnerMesh->m_TerrainGenerator.GetTerrainColorData(pos, (float)m_pOwnerMesh->m_pBody->m_RenderParams.m_Radius);
							vert.color_data = glm::vec2(colorData.elevation, colorData.mountain_noise);

							m_vGeneratedVertices.push_back(vert);
							unsigned int newIndex = m_vGeneratedVertices.size() - 1;
							mVertexMap[key] = newIndex;
							aEdgeVertexIndices[i] = newIndex;
						}
						else
							aEdgeVertexIndices[i] = mVertexMap[key];
					}
				}

				for(int i = 0; MarchingCubesData::ms_TriTable[CubeIndex][i] != -1; i += 3)
				{
					m_vGeneratedIndices.push_back(aEdgeVertexIndices[MarchingCubesData::ms_TriTable[CubeIndex][i]]);
					m_vGeneratedIndices.push_back(aEdgeVertexIndices[MarchingCubesData::ms_TriTable[CubeIndex][i + 1]]);
					m_vGeneratedIndices.push_back(aEdgeVertexIndices[MarchingCubesData::ms_TriTable[CubeIndex][i + 2]]);
				}
			}
		}
	}

	m_bHasGeneratedData = true;
	m_bIsGenerating = false;
}

void COctreeNode::ApplyMeshBuffers()
{
	if(m_EBO != 0)
		glDeleteBuffers(1, &m_EBO);
	if(m_VBO != 0)
		glDeleteBuffers(1, &m_VBO);
	if(m_VAO != 0)
		glDeleteVertexArrays(1, &m_VAO);

	m_NumIndices = m_vGeneratedIndices.size();

	if(m_NumIndices == 0)
	{
		m_VAO = 0;
		m_VBO = 0;
		m_EBO = 0;
		m_bHasGeneratedData = false;
		m_bGenerationAttempted = true;
		return;
	}

	glGenVertexArrays(1, &m_VAO);
	glGenBuffers(1, &m_VBO);
	glGenBuffers(1, &m_EBO);

	glBindVertexArray(m_VAO);

	glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
	glBufferData(GL_ARRAY_BUFFER, m_vGeneratedVertices.size() * sizeof(SProceduralVertex), m_vGeneratedVertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_vGeneratedIndices.size() * sizeof(unsigned int), m_vGeneratedIndices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SProceduralVertex), (void *)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SProceduralVertex), (void *)offsetof(SProceduralVertex, normal));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SProceduralVertex), (void *)offsetof(SProceduralVertex, texCoord));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(SProceduralVertex), (void *)offsetof(SProceduralVertex, color_data));
	glEnableVertexAttribArray(3);

	glBindVertexArray(0);

	m_vGeneratedVertices.clear();
	m_vGeneratedIndices.clear();
	m_bHasGeneratedData = false;
	m_bGenerationAttempted = true;
}

void COctreeNode::Update(CCamera &Camera)
{
	Vec3 CamAbsWorldPos = Camera.m_AbsolutePosition;
	Vec3 PlanetCenter = m_pOwnerMesh->m_pBody->m_SimParams.m_Position;
	double PlanetRadius = m_pOwnerMesh->m_pBody->m_RenderParams.m_Radius;

	double Distance;
	if(m_Center.x == 0.0 && m_Center.y == 0.0 && m_Center.z == 0.0)
	{
		double distToPlanetCenter = (CamAbsWorldPos - PlanetCenter).length();
		Distance = std::max(0.0, distToPlanetCenter - PlanetRadius);
	}
	else
	{
		Vec3 Direction = Vec3(m_Center).normalize();
		Vec3 PointOnSurface = PlanetCenter + Direction * PlanetRadius;
		Distance = (PointOnSurface - CamAbsWorldPos).length();
	}
	Vec3 NodeAbsWorldPos = m_pOwnerMesh->m_pBody->m_SimParams.m_Position + Vec3(m_Center);
	Vec3 PlanetToNodeVec = NodeAbsWorldPos - m_pOwnerMesh->m_pBody->m_SimParams.m_Position;
	Vec3 PlanetToCamVec = CamAbsWorldPos - m_pOwnerMesh->m_pBody->m_SimParams.m_Position;
	double Dot = glm::dot(glm::normalize(glm::vec3(PlanetToCamVec)), glm::normalize(glm::vec3(PlanetToNodeVec)));
	double LODPenalty = Dot < 0.0 ? 100.0 : 1.0;
	double LODMetric = (Distance / m_Size) * LODPenalty * 0.1;
	// minimum LOD level
	if(m_Level <= 1)
		LODMetric = 0;
	const double LOD_SPLIT_THRESHOLD = 1.2;
	const double LOD_MERGE_THRESHOLD = 2.0;

	// if(m_pOwnerMesh->m_pBody->m_Id == Camera.m_pFocusedBody->m_Id)
	// {
	// 	double val = LODMetric;
	// 	if(val < Camera.m_LowestDist)
	// 		Camera.m_LowestDist = val;
	// 	if(val > Camera.m_HighestDist)
	// 		Camera.m_HighestDist = val;
	// }

	if(m_bIsLeaf)
	{
		if(LODMetric < LOD_SPLIT_THRESHOLD && m_Level < MAX_LOD_LEVEL)
		{
			// printf("metric:%.6f | vao:%d, gen:%d, data:%d, att:%d\n", LODMetric, m_VAO, (bool)m_bIsGenerating, (bool)m_bHasGeneratedData, (bool)m_Lead_GenerationAttempted);
			if(m_VAO != 0)
			{
				Subdivide();
				for(int i = 0; i < 8; ++i)
					m_pChildren[i]->Update(Camera);
			}
			else if(!m_bIsGenerating && !m_bHasGeneratedData && !m_bGenerationAttempted)
			{
				m_bIsGenerating = true;
				m_pOwnerMesh->AddToGenerationQueue(shared_from_this());
			}
		}
		else if(m_VAO == 0 && !m_bIsGenerating && !m_bHasGeneratedData && !m_bGenerationAttempted)
		{
			m_bIsGenerating = true;
			m_pOwnerMesh->AddToGenerationQueue(shared_from_this());
		}
	}
	else
	{
		if(LODMetric > LOD_MERGE_THRESHOLD && m_Level > 0)
			Merge();
		else
			for(int i = 0; i < 8; ++i)
				m_pChildren[i]->Update(Camera);
	}
}

void COctreeNode::Render(CShader &Shader, const Vec3 &CameraAbsolutePos, const Vec3 &PlanetAbsolutePos)
{
	Vec3 nodeAbsolutePos_d = PlanetAbsolutePos + Vec3(m_Center);
	Vec3 nodeCamRelativePos_d = nodeAbsolutePos_d - CameraAbsolutePos;
	glm::mat4 NodeModel = glm::translate(glm::mat4(1.0f), (glm::vec3)nodeCamRelativePos_d);

	if(m_bIsLeaf)
	{
		if(m_VAO != 0 && m_NumIndices > 0)
		{
			Shader.SetMat4("uModel", NodeModel);
			glBindVertexArray(m_VAO);
			glDrawElements(GL_TRIANGLES, m_NumIndices, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);
		}
	}
	else
	{
		bool bChildrenReady = true;
		for(int i = 0; i < 8; ++i)
		{
			if(m_pChildren[i] && m_pChildren[i]->m_bIsLeaf && m_pChildren[i]->m_VAO == 0 && !m_pChildren[i]->m_bGenerationAttempted)
			{
				bChildrenReady = false;
				break;
			}
		}

		if(bChildrenReady)
		{
			for(int i = 0; i < 8; ++i)
				if(m_pChildren[i])
					m_pChildren[i]->Render(Shader, CameraAbsolutePos, PlanetAbsolutePos);
		}
		else
		{
			// Children not ready, render self
			if(m_VAO != 0 && m_NumIndices > 0)
			{
				Shader.SetMat4("uModel", NodeModel);
				glBindVertexArray(m_VAO);
				glDrawElements(GL_TRIANGLES, m_NumIndices, GL_UNSIGNED_INT, 0);
				glBindVertexArray(0);
			}
		}
	}
}

void COctreeNode::Subdivide()
{
	if(!m_bIsLeaf)
		return;

	m_bIsLeaf = false;

	float newSize = m_Size * 0.5f;
	float offset = m_Size * 0.25f; // Offset from parent center to child center

	// printf("Creating new children at distance: %.2f\n", glm::length(glm::vec2(m_Center.x + offset, m_Center.y + offset)));
	// Create 8 new child nodes
	m_pChildren[0] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(-offset, -offset, -offset), newSize, m_Level + 1, m_VoxelResolution); // ---
	m_pChildren[1] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(+offset, -offset, -offset), newSize, m_Level + 1, m_VoxelResolution); // +--
	m_pChildren[2] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(+offset, +offset, -offset), newSize, m_Level + 1, m_VoxelResolution); // ++-
	m_pChildren[3] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(-offset, +offset, -offset), newSize, m_Level + 1, m_VoxelResolution); // -+-
	m_pChildren[4] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(-offset, -offset, +offset), newSize, m_Level + 1, m_VoxelResolution); // --+
	m_pChildren[5] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(+offset, -offset, +offset), newSize, m_Level + 1, m_VoxelResolution); // +-+
	m_pChildren[6] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(+offset, +offset, +offset), newSize, m_Level + 1, m_VoxelResolution); // +++
	m_pChildren[7] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(-offset, +offset, +offset), newSize, m_Level + 1, m_VoxelResolution); // -++
}

void COctreeNode::Merge()
{
	if(m_bIsLeaf)
		return;

	for(int i = 0; i < 8; ++i) // Delete all 8 children
		m_pChildren[i] = nullptr;
	m_bIsLeaf = true;

	// Request mesh generation for this node (the new leaf)
	if(m_VAO == 0 && !m_bIsGenerating)
	{
		m_bIsGenerating = true;
		m_bGenerationAttempted = false;
		m_bHasGeneratedData = false;
		m_pOwnerMesh->AddToGenerationQueue(shared_from_this());
	}
}
