#include "proceduralmesh.h"
#include "generated/embedded_shaders.h"
#include "marchingcubes.h"
#include <cstdio>
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
	m_pCaveNoise = new FastNoiseLite();
	m_pMountainNoise = new FastNoiseLite();
	m_bRunWorker = true;
}

CProceduralMesh::~CProceduralMesh()
{
	Destroy();
	delete m_pCaveNoise;
	delete m_pMountainNoise;
}

void CProceduralMesh::Init(SBody *pBody)
{
	m_pBody = pBody;

	m_Shader.CompileShader(Shaders::VERT_BODY, Shaders::FRAG_BODY);

	m_pCaveNoise->SetSeed(m_pBody->m_Id);
	m_pCaveNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pCaveNoise->SetFrequency(0.1f);
	m_pCaveNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pCaveNoise->SetFractalOctaves(3);

	m_pMountainNoise->SetSeed(m_pBody->m_Id + 1);
	m_pMountainNoise->SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
	m_pMountainNoise->SetFrequency(0.05f);
	m_pMountainNoise->SetFractalType(FastNoiseLite::FractalType_FBm);
	m_pMountainNoise->SetFractalOctaves(6);

	float RootSize = (float)m_pBody->m_RenderParams.m_Radius * 2.0f;
	m_pRootNode = std::make_shared<COctreeNode>(this, std::weak_ptr<COctreeNode>(), glm::vec3(0.0f), RootSize, 0);

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
	m_Shader.Use();

	Vec3 relativePos = (m_pBody->m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position);

	glm::mat4 ScaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(1.0 / Camera.m_ViewDistance));
	glm::mat4 TransMat = glm::translate(glm::mat4(1.0f), (glm::vec3)relativePos);
	glm::mat4 Model = ScaleMat * TransMat;

	m_Shader.SetBool("uSource", m_pBody == pLightBody);

	m_Shader.SetMat4("uModel", Model);
	m_Shader.SetMat4("uView", Camera.m_View);
	m_Shader.SetMat4("uProjection", Camera.m_Projection);

	Vec3 NewLightDir = (pLightBody->m_SimParams.m_Position - Camera.m_pFocusedBody->m_SimParams.m_Position).normalize();
	m_Shader.SetVec3("uLightDir", (glm::vec3)NewLightDir);
	m_Shader.SetVec3("uLightColor", pLightBody->m_RenderParams.m_Color);
	m_Shader.SetVec3("uObjectColor", m_pBody->m_RenderParams.m_Color);

	// Render the single root node
	if(m_pRootNode)
		m_pRootNode->Render(m_Shader);
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

COctreeNode::COctreeNode(CProceduralMesh *pOwnerMesh, std::weak_ptr<COctreeNode> pParent, glm::vec3 center, float size, int level) :
	m_pOwnerMesh(pOwnerMesh),
	m_pParent(pParent),
	m_Level(level),
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
	const int res = VOXEL_RESOLUTION;
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
				vDensityGrid[idx] = m_pOwnerMesh->GetTerrainDensity(world_pos);
				vGradientGrid[idx] = m_pOwnerMesh->CalculateDensityGradient(world_pos);
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
							vert.position = pos;
							vert.normal = norm;
							vert.texCoord = glm::vec2(pos.x, pos.z) / 1000.0f;

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

	glBindVertexArray(0);

	m_vGeneratedVertices.clear();
	m_vGeneratedIndices.clear();
	m_bHasGeneratedData = false;
	m_bGenerationAttempted = true;
}

void COctreeNode::Update(CCamera &Camera)
{
	Vec3 NodeAbsWorldPos = m_pOwnerMesh->m_pBody->m_SimParams.m_Position + (Vec3(m_Center) / 1000.f);
	Vec3 CamRelativeWorldPos = (Vec3(Camera.m_Position) / DEFAULT_SCALE) * Camera.m_ViewDistance;
	Vec3 CamAbsWorldPos = Camera.m_pFocusedBody->m_SimParams.m_Position + CamRelativeWorldPos;

	double DistanceToNode = (NodeAbsWorldPos - CamAbsWorldPos).length();

	Vec3 PlanetToCamVec = (CamAbsWorldPos - Camera.m_pFocusedBody->m_SimParams.m_Position);
	Vec3 PlanetToNodeVec = (NodeAbsWorldPos - Camera.m_pFocusedBody->m_SimParams.m_Position);

	double Dot = glm::dot(
		glm::normalize(glm::vec3(PlanetToCamVec)),
		glm::normalize(glm::vec3(PlanetToNodeVec)));

	double lodPenalty = 1.0;
	if(Dot < 0.0)
		lodPenalty = 100.0;
	else if(Dot < 1.0)
	{
		double Multiplier = ((Camera.m_pFocusedBody->m_RenderParams.m_Radius - Camera.m_ViewDistance) / (Camera.m_pFocusedBody->m_RenderParams.m_Radius * 0.01));
		// printf("Mulitplier: %.2f\n", Multiplier);
		lodPenalty = 1.0 + (1.0 - Dot) * Multiplier;
	}

	double Score = (DistanceToNode / m_Size) * lodPenalty;
	double LODMetric = Score;
	const double LOD_SPLIT_THRESHOLD = 0.5;
	const double LOD_MERGE_THRESHOLD = 2.0;

	// Debugging values
	if(m_pOwnerMesh->m_pBody->m_Id == Camera.m_pFocusedBody->m_Id)
	{
		double val = LODMetric;
		if(val < Camera.m_LowestDist)
			Camera.m_LowestDist = val;
		if(val > Camera.m_HighestDist)
			Camera.m_HighestDist = val;
	}

	if(m_bIsLeaf)
	{
		if(LODMetric < LOD_SPLIT_THRESHOLD && m_Level < MAX_LOD_LEVEL)
		{
			printf("metric:%.6f | vao:%d, gen:%d, data:%d, att:%d\n", LODMetric, m_VAO, (bool)m_bIsGenerating, (bool)m_bHasGeneratedData, (bool)m_bGenerationAttempted);
			if(m_VAO != 0)
			{
				Subdivide();
				for(int i = 0; i < 8; ++i)
					m_pChildren[i]->Update(Camera);
			}
			else if(!m_bIsGenerating && !m_bHasGeneratedData && !m_bGenerationAttempted)
			{
				m_bIsGenerating = true;
				m_bGenerationAttempted = false;
				m_pOwnerMesh->AddToGenerationQueue(shared_from_this());
			}
		}
		else if(m_VAO == 0 && !m_bIsGenerating && !m_bHasGeneratedData && !m_bGenerationAttempted)
		{
			m_bIsGenerating = true;
			m_bGenerationAttempted = false;
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

void COctreeNode::Render(CShader &Shader)
{
	if(m_bIsLeaf)
	{
		if(m_VAO != 0 && m_NumIndices > 0)
		{
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
			for(int i = 0; i < 8; ++i) // Render all 8 children
				if(m_pChildren[i])
					m_pChildren[i]->Render(Shader);
		}
		else
		{
			// Children not ready, render self
			if(m_VAO != 0 && m_NumIndices > 0)
			{
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
	m_pChildren[0] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(-offset, -offset, -offset), newSize, m_Level + 1); // ---
	m_pChildren[1] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(+offset, -offset, -offset), newSize, m_Level + 1); // +--
	m_pChildren[2] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(+offset, +offset, -offset), newSize, m_Level + 1); // ++-
	m_pChildren[3] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(-offset, +offset, -offset), newSize, m_Level + 1); // -+-
	m_pChildren[4] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(-offset, -offset, +offset), newSize, m_Level + 1); // --+
	m_pChildren[5] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(+offset, -offset, +offset), newSize, m_Level + 1); // +-+
	m_pChildren[6] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(+offset, +offset, +offset), newSize, m_Level + 1); // +++
	m_pChildren[7] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + glm::vec3(-offset, +offset, +offset), newSize, m_Level + 1); // -++
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
