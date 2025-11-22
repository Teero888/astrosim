#include "proceduralmesh.h"
#include "camera.h"
#include "glm/geometric.hpp"
#include "marchingcubes.h"
#include <algorithm>
#include <embedded_shaders.h>

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <unordered_map>

// =========================================================
// HELPER FUNCTIONS
// =========================================================

// Calculate Frustum Planes from View*Projection Matrix
void CProceduralMesh::CalculateFrustum(const CCamera &Camera)
{
	glm::mat4 VP = Camera.m_Projection * Camera.m_View;

	m_FrustumPlanes[0] = glm::row(VP, 3) + glm::row(VP, 0); // Left
	m_FrustumPlanes[1] = glm::row(VP, 3) - glm::row(VP, 0); // Right
	m_FrustumPlanes[2] = glm::row(VP, 3) + glm::row(VP, 1); // Bottom
	m_FrustumPlanes[3] = glm::row(VP, 3) - glm::row(VP, 1); // Top
	m_FrustumPlanes[4] = glm::row(VP, 3) + glm::row(VP, 2); // Near
	m_FrustumPlanes[5] = glm::row(VP, 3) - glm::row(VP, 2); // Far

	for(int i = 0; i < 6; ++i)
	{
		float Length = glm::length(glm::vec3(m_FrustumPlanes[i]));
		m_FrustumPlanes[i] /= Length;
	}
}

// Checks if a Sphere (in Camera-Relative World Space) is inside the frustum
bool IsSphereInFrustum(const std::array<glm::vec4, 6> &planes, const glm::vec3 &center, float radius)
{
	for(int i = 0; i < 6; ++i)
	{
		float Dist = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
		if(Dist < -radius)
			return false;
	}
	return true;
}

// Distance to Axis Aligned Bounding Box (All inputs must be in the SAME Local Space)
double GetDistanceToBox(const Vec3 &pointLocal, const Vec3 &boxCenterLocal, double boxSize)
{
	double HalfSize = boxSize * 0.5;
	// Calculate delta from the box surface
	double dx = std::max(0.0, std::abs(pointLocal.x - boxCenterLocal.x) - HalfSize);
	double dy = std::max(0.0, std::abs(pointLocal.y - boxCenterLocal.y) - HalfSize);
	double dz = std::max(0.0, std::abs(pointLocal.z - boxCenterLocal.z) - HalfSize);

	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool IsChunkOccluded(const Vec3 &chunkCenterRelPlanet, double chunkSize, const Vec3 &camPosRelPlanet, double planetRadius)
{
	double DistToCam = camPosRelPlanet.length();
	// Don't cull if we are very close (on top of the chunk)
	if(DistToCam < planetRadius + chunkSize * 4.0)
		return false;

	// Horizon Culling Math
	double HorizonDistFromCenter = (planetRadius * planetRadius) / DistToCam;
	Vec3 CamDir = camPosRelPlanet / DistToCam;
	double ProjectedDist = chunkCenterRelPlanet.dot(CamDir);
	double ChunkBoundingRadius = chunkSize * 0.9; // Conservative radius

	// If the chunk is "below" the horizon line relative to the camera
	if(ProjectedDist + ChunkBoundingRadius < HorizonDistFromCenter)
		return true;

	return false;
}

// =========================================================
// CProceduralMesh Implementation
// =========================================================

namespace std {
template<typename T, typename U>
struct hash<pair<T, U>>
{
	size_t operator()(const pair<T, U> &p) const
	{
		auto h1 = hash<T>{}(p.first);
		auto h2 = hash<U>{}(p.second);
		return h1 ^ (h2 << 1);
	}
};
} // namespace std

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

	m_Shader.CompileShader(Shaders::VERT_BODY, Shaders::FRAG_BODY);

	// Initialize Debug Shader and Buffers
	m_DebugShader.CompileShader(Shaders::VERT_SOLID, Shaders::FRAG_SOLID);
	InitDebug();

	if(m_BodyType == EBodyType::TERRESTRIAL)
		m_TerrainGenerator.Init(pBody->m_Id + pBody->m_RenderParams.m_Seed, pBody->m_RenderParams.m_Terrain, pBody->m_RenderParams.m_TerrainType);

	if(m_BodyType == EBodyType::TERRESTRIAL || m_BodyType == EBodyType::STAR)
	{
		const auto &terrainParams = m_pBody->m_RenderParams.m_Terrain;
		float max_displacement_factor = terrainParams.m_ContinentHeight + terrainParams.m_MountainHeight + terrainParams.m_HillsHeight + terrainParams.m_DetailHeight;
		float scale_factor = 1.0f + max_displacement_factor * 1.2f;

		double RootSize = m_pBody->m_RenderParams.m_Radius * 2.0 * (double)scale_factor;
		m_pRootNode = std::make_shared<COctreeNode>(this, std::weak_ptr<COctreeNode>(), Vec3(0.0), RootSize, 0, voxelResolution);
	}

	unsigned int NumThreads = std::thread::hardware_concurrency();
	NumThreads = (NumThreads > 1) ? NumThreads - 1 : 1;

	for(unsigned int i = 0; i < NumThreads; ++i)
		m_vWorkerThreads.emplace_back(&CProceduralMesh::GenerationWorkerLoop, this);
}

void CProceduralMesh::InitDebug()
{
	float vertices[] = {
		-0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
		-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f};
	unsigned int indices[] = {
		0, 1, 1, 2, 2, 3, 3, 0, // Back face
		4, 5, 5, 6, 6, 7, 7, 4, // Front face
		0, 4, 1, 5, 2, 6, 3, 7 // Connecting lines
	};
	glGenVertexArrays(1, &m_DebugCubeVAO);
	glGenBuffers(1, &m_DebugCubeVBO);
	glGenBuffers(1, &m_DebugCubeEBO);
	glBindVertexArray(m_DebugCubeVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_DebugCubeVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_DebugCubeEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
}

void CProceduralMesh::RenderDebug(const CCamera &Camera)
{
	if(!m_bVisualizeOctree || !m_pRootNode)
		return;

	glDisable(GL_DEPTH_TEST);

	m_DebugShader.Use();
	m_DebugShader.SetMat4("uView", Camera.m_View);
	m_DebugShader.SetMat4("uProjection", Camera.m_Projection);

	Quat q = m_pBody->m_SimParams.m_Orientation;
	glm::quat glmQ(q.w, q.x, q.y, q.z);
	glm::mat4 rotationMat = glm::mat4_cast(glmQ);

	std::function<void(COctreeNode *)> DrawNode = [&](COctreeNode *node) {
		if(!node)
			return;

		if(node->m_bIsLeaf || node->m_VAO != 0)
		{
			Vec3 PlanetToCam = m_pBody->m_SimParams.m_Position - Camera.m_AbsolutePosition;
			Vec3 NodeCenterWorld = q.RotateVector(node->m_Center);
			Vec3 NodeToCam = PlanetToCam + NodeCenterWorld;
			glm::mat4 MatTranslate = glm::translate(glm::mat4(1.0f), (glm::vec3)NodeToCam);
			glm::mat4 MatScale = glm::scale(glm::mat4(1.0f), glm::vec3(node->m_Size));

			glm::mat4 Model = MatTranslate * rotationMat * MatScale;

			m_DebugShader.SetMat4("uModel", Model);

			glBindVertexArray(m_DebugCubeVAO);
			glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
		}

		if(!node->m_bIsLeaf)
		{
			for(int i = 0; i < 8; ++i)
				DrawNode(node->m_pChildren[i].get());
		}
	};

	DrawNode(m_pRootNode.get());
	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);
}

void CProceduralMesh::Update(CCamera &Camera)
{
	CheckApplyQueue();

	CalculateFrustum(Camera);

	if(m_pRootNode)
		m_pRootNode->Update(Camera);
}

void CProceduralMesh::Render(const CCamera &Camera, const SBody *pLightBody, bool bIsShadowPass)
{
	if(!m_pRootNode)
		return;

	m_Shader.Use();

	m_Shader.SetBool("uIsShadowPass", bIsShadowPass);

	float F = 2.0f / log2(FAR_PLANE + 1.0f);
	m_Shader.SetFloat("u_logDepthF", F);

	m_Shader.SetBool("uSource", m_pBody == pLightBody);

	m_Shader.SetMat4("uView", Camera.m_View);
	m_Shader.SetMat4("uProjection", Camera.m_Projection);

	Vec3 relativeCamPos = Camera.m_AbsolutePosition - m_pBody->m_SimParams.m_Position;
	m_Shader.SetVec3("uViewPos", glm::vec3(0.0f));
	m_Shader.SetVec3("uPlanetCenterRelCam", (glm::vec3)(-relativeCamPos));
	m_Shader.SetFloat("uAmbientStrength", 0.1f);

	m_Shader.SetFloat("uSpecularStrength", 0.05f);
	m_Shader.SetFloat("uShininess", 32.0f);

	Vec3 LightDir = (pLightBody->m_SimParams.m_Position - m_pBody->m_SimParams.m_Position).normalize();
	m_Shader.SetVec3("uLightDir", (glm::vec3)LightDir);
	m_Shader.SetVec3("uLightColor", pLightBody->m_RenderParams.m_Color);
	m_Shader.SetVec3("uObjectColor", m_pBody->m_RenderParams.m_Color);

	m_Shader.SetInt("uTerrainType", (int)m_pBody->m_RenderParams.m_TerrainType);
	m_Shader.SetFloat("uPlanetRadius", (float)m_pBody->m_RenderParams.m_Radius);

	if(m_pBody->m_RenderParams.m_Atmosphere.m_Enabled)
	{
		m_Shader.SetVec3("uRayleighScatteringCoeff", m_pBody->m_RenderParams.m_Atmosphere.m_RayleighScatteringCoeff);
		m_Shader.SetFloat("uRayleighScaleHeight", m_pBody->m_RenderParams.m_Atmosphere.m_RayleighScaleHeight);
		m_Shader.SetVec3("uMieScatteringCoeff", m_pBody->m_RenderParams.m_Atmosphere.m_MieScatteringCoeff);
		m_Shader.SetFloat("uMieScaleHeight", m_pBody->m_RenderParams.m_Atmosphere.m_MieScaleHeight);
		m_Shader.SetFloat("uMiePreferredScatteringDir", m_pBody->m_RenderParams.m_Atmosphere.m_MiePreferredScatteringDir);
		m_Shader.SetFloat("uAtmosphereRadius", m_pBody->m_RenderParams.m_Atmosphere.m_AtmosphereRadius);
	}
	else
	{
		m_Shader.SetFloat("uAtmosphereRadius", 1.0f);
	}

	m_Shader.SetVec3("uDeepOcean", m_pBody->m_RenderParams.m_Colors.m_DeepOcean);
	m_Shader.SetVec3("uShallowOcean", m_pBody->m_RenderParams.m_Colors.m_ShallowOcean);
	m_Shader.SetVec3("uBeach", m_pBody->m_RenderParams.m_Colors.m_Beach);
	m_Shader.SetVec3("uGrass", m_pBody->m_RenderParams.m_Colors.m_Grass);
	m_Shader.SetVec3("uForest", m_pBody->m_RenderParams.m_Colors.m_Forest);
	m_Shader.SetVec3("uDesert", m_pBody->m_RenderParams.m_Colors.m_Desert);
	m_Shader.SetVec3("uSnow", m_pBody->m_RenderParams.m_Colors.m_Snow);
	m_Shader.SetVec3("uRock", m_pBody->m_RenderParams.m_Colors.m_Rock);
	m_Shader.SetVec3("uTundra", m_pBody->m_RenderParams.m_Colors.m_Tundra);

	if(m_pRootNode)
		m_pRootNode->Render(m_Shader, Camera.m_AbsolutePosition, m_pBody->m_SimParams.m_Position, m_pBody->m_SimParams.m_Orientation);
}

void CProceduralMesh::Destroy()
{
	m_bRunWorker = false;
	m_GenQueueCV.notify_all();
	m_ApplyQueueCV.notify_all();

	for(auto &thread : m_vWorkerThreads)
	{
		if(thread.joinable())
			thread.join();
	}

	m_pRootNode = nullptr;
	m_Shader.Destroy();
	m_DebugShader.Destroy();
	if(m_DebugCubeVAO)
		glDeleteVertexArrays(1, &m_DebugCubeVAO);
	if(m_DebugCubeVBO)
		glDeleteBuffers(1, &m_DebugCubeVBO);
	if(m_DebugCubeEBO)
		glDeleteBuffers(1, &m_DebugCubeEBO);
}

void CProceduralMesh::AddToGenerationQueue(std::shared_ptr<COctreeNode> pNode, double distToCam)
{
	{
		std::lock_guard<std::mutex> lock(m_GenQueueMutex);
		m_GenerationQueue.push({pNode, distToCam});
	}
	m_GenQueueCV.notify_one();
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
			m_ApplyQueueCV.notify_one();
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
			m_GenQueueCV.wait(lock, [this] {
				return !m_GenerationQueue.empty() || !m_bRunWorker;
			});

			if(!m_bRunWorker)
				break;

			if(!m_GenerationQueue.empty())
			{
				// Get highest priority (shortest distance)
				pNode = m_GenerationQueue.top().m_pNode;
				m_GenerationQueue.pop();
			}
		}

		if(pNode)
		{
			pNode->GenerateMesh();

			{
				std::unique_lock<std::mutex> lock(m_ApplyQueueMutex);

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

COctreeNode::COctreeNode(CProceduralMesh *pOwnerMesh, std::weak_ptr<COctreeNode> pParent, Vec3 center, double size, int level, int voxelResolution) :
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
	const int res = m_VoxelResolution;
	const int Padding = 1;
	const int PaddedRes = res + Padding * 2;
	const int PaddedRes1 = PaddedRes + 1;
	const int NumGridPoints = PaddedRes1 * PaddedRes1 * PaddedRes1;

	std::vector<STerrainOutput> vTerrainGrid(NumGridPoints);

	double StepSize = m_Size / (double)res;
	Vec3 StartCorner = m_Center - Vec3(m_Size * 0.5);
	Vec3 SamplingStartCorner = StartCorner - Vec3((double)Padding * StepSize);

	double radius = m_pOwnerMesh->m_pBody->m_RenderParams.m_Radius;

	for(int z = 0; z <= PaddedRes; ++z)
	{
		for(int y = 0; y <= PaddedRes; ++y)
		{
			for(int x = 0; x <= PaddedRes; ++x)
			{
				Vec3 world_pos = SamplingStartCorner + Vec3((double)x * StepSize, (double)y * StepSize, (double)z * StepSize);
				int idx = x + y * PaddedRes1 + z * PaddedRes1 * PaddedRes1;
				vTerrainGrid[idx] = m_pOwnerMesh->m_TerrainGenerator.GetTerrainOutput(world_pos, radius);
			}
		}
	}

	m_vGeneratedVertices.clear();
	m_vGeneratedIndices.clear();
	std::unordered_map<std::pair<int, int>, unsigned int> mVertexMap;

	const int aaCornerOffsets[8][3] = {
		{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
		{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};

	const int aaEdgeToCorners[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0},
		{4, 5}, {5, 6}, {6, 7}, {7, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}};

	for(int z = Padding; z < Padding + res; ++z)
	{
		for(int y = Padding; y < Padding + res; ++y)
		{
			for(int x = Padding; x < Padding + res; ++x)
			{
				Vec3 Corners[8];
				float Densities[8];
				int CornerGlobalIndices[8];
				int CubeIndex = 0;

				for(int i = 0; i < 8; ++i)
				{
					int dx = aaCornerOffsets[i][0];
					int dy = aaCornerOffsets[i][1];
					int dz = aaCornerOffsets[i][2];
					int idx = (x + dx) + (y + dy) * PaddedRes1 + (z + dz) * PaddedRes1 * PaddedRes1;

					Corners[i] = SamplingStartCorner + Vec3((double)(x + dx) * StepSize, (double)(y + dy) * StepSize, (double)(z + dz) * StepSize);
					Densities[i] = vTerrainGrid[idx].density;
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

						std::pair<int, int> key = (c1_global < c2_global) ? std::make_pair(c1_global, c2_global) : std::make_pair(c2_global, c1_global);

						if(mVertexMap.find(key) == mVertexMap.end())
						{
							Vec3 p1 = Corners[c1_local];
							Vec3 p2 = Corners[c2_local];
							float d1 = Densities[c1_local];
							float d2 = Densities[c2_local];

							float t = (glm::abs(d1 - d2) > 0.00001f) ? (0.0f - d1) / (d2 - d1) : 0.5f;

							Vec3 posDouble = p1 * (1.0 - (double)t) + p2 * (double)t;
							glm::vec3 norm = m_pOwnerMesh->m_TerrainGenerator.CalculateDensityGradient(posDouble, radius);

							STerrainOutput t1 = vTerrainGrid[c1_global];
							STerrainOutput t2 = vTerrainGrid[c2_global];

							float elev = glm::mix(t1.elevation, t2.elevation, t);
							float temp = glm::mix(t1.temperature, t2.temperature, t);
							float moist = glm::mix(t1.moisture, t2.moisture, t);
							float mask = glm::mix(t1.material_mask, t2.material_mask, t);

							SProceduralVertex vert;
							Vec3 localPos = posDouble - m_Center;
							vert.position = (glm::vec3)localPos;
							vert.normal = norm;
							vert.texCoord = glm::vec2((float)posDouble.x, (float)posDouble.z) / 1000.0f;
							vert.color_data = glm::vec4(elev, temp, moist, mask);

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

	// ==========================================
	// SKIRT GENERATION
	// ==========================================
	// This pass detects vertices on the edges of the chunk and generates "skirts"
	// (geometry pointing inwards) to hide gaps between different LOD levels.

	float BoundsLimit = (float)(m_Size * 0.5) * 0.99f;
	float SkirtDepth = (float)(StepSize * 0.5); // Depth of the skirt, proportional to voxel size

	// Helper to determine boundary mask for a position
	// 1: x-, 2: x+, 4: y-, 8: y+, 16: z-, 32: z+
	auto GetBoundaryMask = [&](const glm::vec3 &p) -> int {
		int mask = 0;
		if(p.x < -BoundsLimit)
			mask |= 1;
		if(p.x > BoundsLimit)
			mask |= 2;
		if(p.y < -BoundsLimit)
			mask |= 4;
		if(p.y > BoundsLimit)
			mask |= 8;
		if(p.z < -BoundsLimit)
			mask |= 16;
		if(p.z > BoundsLimit)
			mask |= 32;
		return mask;
	};

	// We iterate only the currently generated triangles before adding skirts
	size_t OriginalIndexCount = m_vGeneratedIndices.size();

	// Map to reuse skirt vertices: key is (original_vertex_index << 6) | boundary_mask
	// This prevents adding duplicate skirt vertices for the same corner
	std::unordered_map<uint64_t, unsigned int> SkirtVertexMap;

	auto GetOrCreateSkirtVertex = [&](unsigned int originalIdx, int boundaryMask) -> unsigned int {
		uint64_t key = ((uint64_t)originalIdx << 6) | (uint64_t)boundaryMask;
		if(SkirtVertexMap.find(key) != SkirtVertexMap.end())
			return SkirtVertexMap[key];

		SProceduralVertex NewVert = m_vGeneratedVertices[originalIdx];
		// Move vertex "down" relative to the surface normal to create a skirt
		NewVert.position -= NewVert.normal * SkirtDepth;

		m_vGeneratedVertices.push_back(NewVert);
		unsigned int newIdx = (unsigned int)m_vGeneratedVertices.size() - 1;
		SkirtVertexMap[key] = newIdx;
		return newIdx;
	};

	for(size_t i = 0; i < OriginalIndexCount; i += 3)
	{
		unsigned int i1 = m_vGeneratedIndices[i];
		unsigned int i2 = m_vGeneratedIndices[i + 1];
		unsigned int i3 = m_vGeneratedIndices[i + 2];

		int m1 = GetBoundaryMask(m_vGeneratedVertices[i1].position);
		int m2 = GetBoundaryMask(m_vGeneratedVertices[i2].position);
		int m3 = GetBoundaryMask(m_vGeneratedVertices[i3].position);

		// Check Edge 1-2
		int EdgeMask = m1 & m2;
		if(EdgeMask != 0)
		{
			unsigned int s1 = GetOrCreateSkirtVertex(i1, EdgeMask);
			unsigned int s2 = GetOrCreateSkirtVertex(i2, EdgeMask);
			// Add Quad (Triangle 1)
			m_vGeneratedIndices.push_back(i1);
			m_vGeneratedIndices.push_back(s1);
			m_vGeneratedIndices.push_back(i2);
			// Add Quad (Triangle 2)
			m_vGeneratedIndices.push_back(i2);
			m_vGeneratedIndices.push_back(s1);
			m_vGeneratedIndices.push_back(s2);
		}

		// Check Edge 2-3
		EdgeMask = m2 & m3;
		if(EdgeMask != 0)
		{
			unsigned int s2 = GetOrCreateSkirtVertex(i2, EdgeMask);
			unsigned int s3 = GetOrCreateSkirtVertex(i3, EdgeMask);
			m_vGeneratedIndices.push_back(i2);
			m_vGeneratedIndices.push_back(s2);
			m_vGeneratedIndices.push_back(i3);
			m_vGeneratedIndices.push_back(i3);
			m_vGeneratedIndices.push_back(s2);
			m_vGeneratedIndices.push_back(s3);
		}

		// Check Edge 3-1
		EdgeMask = m3 & m1;
		if(EdgeMask != 0)
		{
			unsigned int s3 = GetOrCreateSkirtVertex(i3, EdgeMask);
			unsigned int s1 = GetOrCreateSkirtVertex(i1, EdgeMask);
			m_vGeneratedIndices.push_back(i3);
			m_vGeneratedIndices.push_back(s3);
			m_vGeneratedIndices.push_back(i1);
			m_vGeneratedIndices.push_back(i1);
			m_vGeneratedIndices.push_back(s3);
			m_vGeneratedIndices.push_back(s1);
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
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(SProceduralVertex), (void *)offsetof(SProceduralVertex, color_data));
	glEnableVertexAttribArray(3);

	glBindVertexArray(0);

	m_vGeneratedVertices.clear();
	m_vGeneratedIndices.clear();
	m_bHasGeneratedData = false;
	m_bGenerationAttempted = true;
}

void COctreeNode::Update(CCamera &Camera)
{
	bool bSplit;
	bool bMerge;
	double DistToBox = 0.f;

	// split until minimum is hit
	if(m_pOwnerMesh->m_pBody == Camera.m_pFocusedBody && m_Level <= 3)
	{
		bSplit = true;
		bMerge = false;
	}
	else
	{
		Vec3 CamPosRelPlanet = Camera.m_AbsolutePosition - m_pOwnerMesh->m_pBody->m_SimParams.m_Position;

		Quat q = m_pOwnerMesh->m_pBody->m_SimParams.m_Orientation;
		Vec3 CamPosLocal = q.Conjugate().RotateVector(CamPosRelPlanet);

		Vec3 NodeCenterWorld = q.RotateVector(m_Center);
		double sphereRadius = m_Size * 0.9;

		if(m_Level > 0)
		{
			// Horizon Culling Planet Center to Camera vs Chunk
			if(IsChunkOccluded(NodeCenterWorld, m_Size, CamPosRelPlanet, m_pOwnerMesh->m_pBody->m_RenderParams.m_Radius))
			{
				if(!m_bIsLeaf)
					Merge();
				return;
			}

			// Frustum Culling
			Vec3 NodePosRelCam = NodeCenterWorld - CamPosRelPlanet;
			if(!IsSphereInFrustum(m_pOwnerMesh->m_FrustumPlanes, (glm::vec3)NodePosRelCam, (float)sphereRadius))
			{
				if(!m_bIsLeaf)
					Merge();
				return;
			}
		}

		DistToBox = std::max(0.1, GetDistanceToBox(CamPosLocal, m_Center, m_Size));

		double Ratio = m_Size / DistToBox;

		bSplit = Ratio > m_pOwnerMesh->m_SplitMultiplier;
		bMerge = Ratio < m_pOwnerMesh->m_MergeMultiplier;
	}

	if(m_bIsLeaf)
	{
		if(bSplit && m_Level < MAX_LOD_LEVEL)
		{
			if(m_VAO != 0)
			{
				Subdivide();
				for(int i = 0; i < 8; ++i)
					m_pChildren[i]->Update(Camera);
			}
			else if(!m_bIsGenerating && !m_bHasGeneratedData && !m_bGenerationAttempted)
			{
				m_bIsGenerating = true;
				// Add to queue with distance priority!
				m_pOwnerMesh->AddToGenerationQueue(shared_from_this(), DistToBox);
			}
		}
	}
	else
	{
		if(bMerge && m_Level > 0)
			Merge();
		else
			for(int i = 0; i < 8; ++i)
				m_pChildren[i]->Update(Camera);
	}
}

void COctreeNode::Render(CShader &Shader, const Vec3 &CameraAbsolutePos, const Vec3 &PlanetAbsolutePos, const Quat &PlanetOrientation)
{
	Vec3 PlanetToCam = PlanetAbsolutePos - CameraAbsolutePos;
	Vec3 NodeCenterWorld = PlanetOrientation.RotateVector(m_Center);
	Vec3 NodeToCam = PlanetToCam + NodeCenterWorld;
	glm::mat4 MatTranslate = glm::translate(glm::mat4(1.0f), (glm::vec3)NodeToCam);

	glm::quat glmQ(PlanetOrientation.w, PlanetOrientation.x, PlanetOrientation.y, PlanetOrientation.z);
	glm::mat4 MatRotate = glm::mat4_cast(glmQ);

	glm::mat4 NodeModel = MatTranslate * MatRotate;

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
					m_pChildren[i]->Render(Shader, CameraAbsolutePos, PlanetAbsolutePos, PlanetOrientation);
		}
		else
		{
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

	double newSize = m_Size * 0.5;
	double offset = m_Size * 0.25;

	m_pChildren[0] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + Vec3(-offset, -offset, -offset), newSize, m_Level + 1, m_VoxelResolution); // ---
	m_pChildren[1] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + Vec3(+offset, -offset, -offset), newSize, m_Level + 1, m_VoxelResolution); // +--
	m_pChildren[2] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + Vec3(+offset, +offset, -offset), newSize, m_Level + 1, m_VoxelResolution); // ++-
	m_pChildren[3] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + Vec3(-offset, +offset, -offset), newSize, m_Level + 1, m_VoxelResolution); // -+-
	m_pChildren[4] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + Vec3(-offset, -offset, +offset), newSize, m_Level + 1, m_VoxelResolution); // --+
	m_pChildren[5] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + Vec3(+offset, -offset, +offset), newSize, m_Level + 1, m_VoxelResolution); // +-+
	m_pChildren[6] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + Vec3(+offset, +offset, +offset), newSize, m_Level + 1, m_VoxelResolution); // +++
	m_pChildren[7] = std::make_shared<COctreeNode>(m_pOwnerMesh, shared_from_this(), m_Center + Vec3(-offset, +offset, +offset), newSize, m_Level + 1, m_VoxelResolution); // -++
}

void COctreeNode::Merge()
{
	if(m_bIsLeaf)
		return;

	for(int i = 0; i < 8; ++i)
		m_pChildren[i] = nullptr;
	m_bIsLeaf = true;

	if(m_VAO == 0 && !m_bIsGenerating)
	{
		m_bIsGenerating = true;
		m_bGenerationAttempted = false;
		m_bHasGeneratedData = false;
		m_pOwnerMesh->AddToGenerationQueue(shared_from_this(), 0.0); // Merge fallback
	}
}
