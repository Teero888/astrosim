#include "proceduralmesh.h"
#include "camera.h"
#include "glm/geometric.hpp"
#include "marchingcubes.h"
#include <algorithm>
#include <embedded_shaders.h>

#include <glm/gtc/matrix_access.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <unordered_map>

// =========================================================
// CULLING HELPER FUNCTIONS
// =========================================================

// Calculate Frustum Planes from View*Projection Matrix
// Resulting planes are in the same space as the View Matrix (Camera-Relative World Space)
void CProceduralMesh::CalculateFrustum(const CCamera &Camera)
{
	glm::mat4 VP = Camera.m_Projection * Camera.m_View;

	// Left
	m_FrustumPlanes[0] = glm::row(VP, 3) + glm::row(VP, 0);
	// Right
	m_FrustumPlanes[1] = glm::row(VP, 3) - glm::row(VP, 0);
	// Bottom
	m_FrustumPlanes[2] = glm::row(VP, 3) + glm::row(VP, 1);
	// Top
	m_FrustumPlanes[3] = glm::row(VP, 3) - glm::row(VP, 1);
	// Near
	m_FrustumPlanes[4] = glm::row(VP, 3) + glm::row(VP, 2);
	// Far
	m_FrustumPlanes[5] = glm::row(VP, 3) - glm::row(VP, 2);

	// Normalize planes
	for(int i = 0; i < 6; ++i)
	{
		float length = glm::length(glm::vec3(m_FrustumPlanes[i]));
		m_FrustumPlanes[i] /= length;
	}
}

// Check if a sphere is inside the frustum
// Center is in Camera-Relative Space
bool IsSphereInFrustum(const std::array<glm::vec4, 6> &planes, const glm::vec3 &center, float radius)
{
	for(int i = 0; i < 6; ++i)
	{
		float dist = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
		if(dist < -radius)
			return false; // Fully outside this plane
	}
	return true;
}

// Horizon Culling: Returns true if the chunk is completely hidden by the planet curvature
bool IsChunkOccluded(const Vec3 &chunkCenterRelPlanet, double chunkSize, const Vec3 &camPosRelPlanet, double planetRadius)
{
	// 1. Calculate distance from planet center to camera
	double distToCam = camPosRelPlanet.length();

	// If camera is inside the "horizon limit" (close to surface + chunk buffer), don't cull.
	// This prevents culling chunks when we are standing on them.
	if(distToCam < planetRadius + chunkSize * 2.0)
		return false;

	// 2. Calculate Horizon Distance (Distance from Planet Center to the Horizon Plane)
	// Horizon Plane is perpendicular to the vector P->C.
	// dist_horizon = R^2 / D
	double horizonDistFromCenter = (planetRadius * planetRadius) / distToCam;

	// 3. Project Chunk Center onto the Planet->Camera axis
	// P_proj = Dot(ChunkVec, Normalize(CamVec))
	Vec3 camDir = camPosRelPlanet / distToCam; // Normalize
	double projectedDist = chunkCenterRelPlanet.dot(camDir);

	// 4. Check if Chunk is "below" the horizon plane
	// We add the chunk's bounding radius to be conservative.
	// Bounding radius of a cube is size * sqrt(3) / 2 ~= size * 0.866
	double chunkBoundingRadius = chunkSize * 0.87;

	// If the furthest point of the chunk is still closer to the planet center than the horizon plane,
	// AND it is on the front-facing hemisphere side relative to the camera axis (handled by dot product sign logic implicitly),
	// actually, we just need to check if it's "behind" the plane defined by horizonDistFromCenter.
	// Since the camera is at distance D (where D > horizonDist), "behind" means < horizonDist.
	if(projectedDist + chunkBoundingRadius < horizonDistFromCenter)
	{
		return true; // Occluded
	}

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

	if(m_BodyType == EBodyType::TERRESTRIAL)
		m_TerrainGenerator.Init(pBody->m_Id + pBody->m_RenderParams.m_Seed, pBody->m_RenderParams.m_Terrain, pBody->m_RenderParams.m_TerrainType);

	if(m_BodyType == EBodyType::TERRESTRIAL || m_BodyType == EBodyType::STAR)
	{
		const auto &terrainParams = m_pBody->m_RenderParams.m_Terrain;
		float max_displacement_factor = terrainParams.m_ContinentHeight + terrainParams.m_MountainHeight + terrainParams.m_HillsHeight + terrainParams.m_DetailHeight;
		float scale_factor = 1.0f + max_displacement_factor * 1.2f;

		// Use double precision for root size calculation
		double RootSize = m_pBody->m_RenderParams.m_Radius * 2.0 * (double)scale_factor;
		m_pRootNode = std::make_shared<COctreeNode>(this, std::weak_ptr<COctreeNode>(), Vec3(0.0), RootSize, 0, voxelResolution);
	}

	unsigned int NumThreads = std::thread::hardware_concurrency();
	NumThreads = (NumThreads > 1) ? NumThreads - 1 : 1;

	for(unsigned int i = 0; i < NumThreads; ++i)
		m_vWorkerThreads.emplace_back(&CProceduralMesh::GenerationWorkerLoop, this);
}

void CProceduralMesh::Update(CCamera &Camera)
{
	CheckApplyQueue();

	CalculateFrustum(Camera);

	if(m_pRootNode)
		m_pRootNode->Update(Camera);
}

void CProceduralMesh::Render(const CCamera &Camera, const SBody *pLightBody)
{
	if(!m_pRootNode)
		return;

	m_Shader.Use();

	float F = 2.0f / log2(FAR_PLANE + 1.0f);
	m_Shader.SetFloat("u_logDepthF", F);

	m_Shader.SetBool("uSource", m_pBody == pLightBody);

	m_Shader.SetMat4("uView", Camera.m_View);
	m_Shader.SetMat4("uProjection", Camera.m_Projection);

	Vec3 relativeCamPos = Camera.m_AbsolutePosition - m_pBody->m_SimParams.m_Position;
	m_Shader.SetVec3("uViewPos", (glm::vec3)relativeCamPos);
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
		m_pRootNode->Render(m_Shader, Camera.m_AbsolutePosition, m_pBody->m_SimParams.m_Position);
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
}

void CProceduralMesh::AddToGenerationQueue(std::shared_ptr<COctreeNode> pNode)
{
	{
		std::lock_guard<std::mutex> lock(m_GenQueueMutex);
		m_GenerationQueue.push(pNode);
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
				pNode = m_GenerationQueue.front();
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

// Constructor now takes Vec3 center and double size
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

	// Use double precision for grid stepping
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
				// High Precision World Position
				Vec3 world_pos = SamplingStartCorner + Vec3((double)x * StepSize, (double)y * StepSize, (double)z * StepSize);

				int idx = x + y * PaddedRes1 + z * PaddedRes1 * PaddedRes1;

				// Pass double precision vector to Terrain Generator
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
				// Corners must be Vec3 (double)
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

							// Interpolate position in Double Precision
							Vec3 posDouble = p1 * (1.0 - (double)t) + p2 * (double)t;

							// Analytic Normal Calculation
							glm::vec3 norm = m_pOwnerMesh->m_TerrainGenerator.CalculateDensityGradient(posDouble, radius);

							STerrainOutput t1 = vTerrainGrid[c1_global];
							STerrainOutput t2 = vTerrainGrid[c2_global];

							float elev = glm::mix(t1.elevation, t2.elevation, t);
							float temp = glm::mix(t1.temperature, t2.temperature, t);
							float moist = glm::mix(t1.moisture, t2.moisture, t);
							float mask = glm::mix(t1.material_mask, t2.material_mask, t);

							SProceduralVertex vert;

							// Subtract Node Center from Absolute Position.
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
	// 1. Get Relative Positions (Double Precision)
	// NodeCenter is relative to PlanetCenter.
	// PlanetCenter relative to PlanetCenter is (0,0,0).
	// Camera relative to PlanetCenter:
	Vec3 CamPosRelPlanet = Camera.m_AbsolutePosition - m_pOwnerMesh->m_pBody->m_SimParams.m_Position;

	// 2. Horizon Culling
	// We only cull nodes that are definitely not the root (Level 0), and we double check the radius.
	if(m_Level > 0 && IsChunkOccluded(m_Center, m_Size, CamPosRelPlanet, m_pOwnerMesh->m_pBody->m_RenderParams.m_Radius))
	{
		if(!m_bIsLeaf)
			Merge();
		return;
	}

	// 3. Frustum Culling
	// Frustum planes are in Camera-Relative space.
	// So we need the node's center in Camera-Relative space.
	// NodePos_Rel_Cam = NodePos_Rel_Planet - CamPos_Rel_Planet
	Vec3 NodePosRelCam = m_Center - CamPosRelPlanet;

	// Bounding Sphere Radius approximation for a cube
	float sphereRadius = (float)(m_Size * 0.87); // 0.5 * sqrt(3)

	// Skip culling for root or very close nodes to be safe
	if(m_Level > 0 && !IsSphereInFrustum(m_pOwnerMesh->m_FrustumPlanes, (glm::vec3)NodePosRelCam, sphereRadius))
	{
		if(!m_bIsLeaf)
			Merge();
		return;
	}

	// 4. LOD Calculation
	// Calculate distance from Camera to the nearest point on the chunk's surface
	double DistToCenter = NodePosRelCam.length();
	double DistToSurface = std::max(0.0, DistToCenter - sphereRadius);

	// Dot product check for "Behind" nodes (fallback if horizon culling misses edge cases)
	// This helps prioritize splitting nodes facing the camera
	double Dot = glm::dot(glm::normalize((glm::vec3)CamPosRelPlanet), glm::normalize((glm::vec3)m_Center));
	double LODPenalty = (Dot < 0.0) ? 2.0 : 1.0; // Penalize back-facing nodes slightly less aggressively than before

	double LODMetric = (DistToSurface / m_Size) * LODPenalty;

	// At Level 0 or 1, force split to get initial sphere shape
	if(m_Level <= 1)
		LODMetric = 0.0;

	const double LOD_SPLIT_THRESHOLD = 10.0; // Tuned for balance
	const double LOD_MERGE_THRESHOLD = 20.0;

	if(m_bIsLeaf)
	{
		if(LODMetric < LOD_SPLIT_THRESHOLD && m_Level < MAX_LOD_LEVEL)
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
				m_pOwnerMesh->AddToGenerationQueue(shared_from_this());
			}
		}
		// Note: If VAO is 0, we might still be generating. Don't merge.
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
	// 1. Calculate the Node Center in World Space (Double Precision)
	Vec3 nodeAbsolutePos_d = PlanetAbsolutePos + m_Center;

	// 2. Optional: Re-run Frustum Cull for Render (cheaper than Update logic, prevents popping)
	// But generally, if Update is running per-frame, this isn't strictly necessary.
	// We skip it here to trust the Update loop state.

	// 3. Subtract Camera Position (Double Precision)
	Vec3 nodeCamRelativePos_d = nodeAbsolutePos_d - CameraAbsolutePos;

	// 4. Convert to Float for Model Matrix
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
		m_pOwnerMesh->AddToGenerationQueue(shared_from_this());
	}
}
