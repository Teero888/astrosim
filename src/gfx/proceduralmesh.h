#ifndef PROCEDURALMESH_H
#define PROCEDURALMESH_H

#include <GL/glew.h>
#include <atomic>
#include <condition_variable> // Add this include
#include <glm/glm.hpp>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "../sim/body.h"
#include "camera.h"
#include "shader.h"
#include <FastNoiseLite.h>

struct SProceduralVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoord;
};

class COctreeNode; // Forward declare

class CProceduralMesh
{
public:
	friend class COctreeNode; // Change friend class

	CProceduralMesh();
	~CProceduralMesh();

	void Init(SBody *pBody);
	void Update(CCamera &Camera);
	void Render(const CCamera &Camera, const SBody *pLightBody);
	void Destroy();

	float GetTerrainDensity(glm::vec3 worldPosition)
	{
		float PlanetRadius = (float)m_pBody->m_RenderParams.m_Radius; // e.g., 6.371e6 for Earth
		float distance_from_center = glm::length(worldPosition);

		// 1. Base sphere density
		// This is our Signed Distance Field (SDF) for a perfect sphere.
		// It's positive inside the planet and negative outside.
		// The Marching Cubes algorithm will place the surface where density = 0.
		float base_density = PlanetRadius - distance_from_center;

		// 2. Mountain/Continent noise (large scale)
		// We use m_pMountainNoise (freq 0.05f) for large features.
		// We need to scale our worldPosition input so the noise frequency
		// produces realistic-sized features (e.g., 1000km continents).
		// Effective Freq = (Input * Freq)
		// We want Effective Freq = worldPosition / 1,000,000m
		// So: Input * 0.05 = worldPosition / 1,000,000
		//     Input = worldPosition / (1,000,000 * 0.05) = worldPosition / 50,000
		float mountain_coord_scale = 1.0f / 50000.0f;
		float mountain_noise = m_pMountainNoise->GetNoise(worldPosition.x * mountain_coord_scale,
			worldPosition.y * mountain_coord_scale,
			worldPosition.z * mountain_coord_scale);

		// Scale the noise (from -1 to 1) to realistic terrain heights.
		// Earth's terrain varies roughly -11km to +9km. We'll use a 10km amplitude.
		float terrain_amplitude = 10000.0f; // 10km
		float terrain_offset = mountain_noise * terrain_amplitude;

		// 3. Cave noise (small scale, 3D)
		// We use m_pCaveNoise (freq 0.1f) for 3D caves.
		// We want features on a 100m scale.
		// Effective Freq = worldPosition / 100m
		// So: Input * 0.1 = worldPosition / 100
		//     Input = worldPosition / (100 * 0.1) = worldPosition / 10
		float cave_coord_scale = 1.0f / 10.0f;
		float cave_noise = m_pCaveNoise->GetNoise(worldPosition.x * cave_coord_scale,
			worldPosition.y * cave_coord_scale,
			worldPosition.z * cave_coord_scale);

		// Manipulate the cave noise to create hollow pockets.
		// We'll subtract from the density where the noise value is high.
		float cave_effect = 0.0f;
		float cave_threshold = 0.7f; // Creates caves in 30% of the volume
		if(cave_noise > cave_threshold)
		{
			// Smoothly remap (0.7 to 1.0) -> (0.0 to 1.0)
			float remapped_noise = (cave_noise - cave_threshold) / (1.0f - cave_threshold);
			// Subtract from density. Max cave "hollowness" of 50m.
			// This makes the inside of the planet less dense, creating a hole.
			cave_effect = remapped_noise * 50.0f;
		}

		// 4. Combine
		// Density = (Sphere SDF) + (Terrain Height) - (Cave Pockets)
		// - Adding terrain_offset pushes the surface (density=0) outwards (a mountain).
		// - Subtracting cave_effect pulls the surface (density=0) inwards (a cave).
		return base_density; // + terrain_offset - cave_effect;
	}

	glm::vec3 CalculateDensityGradient(glm::vec3 p)
	{
		float eps = glm::length(p) * 0.0001f;
		if(eps < 1e-9f)
			eps = 1e-9f;

		float dx = GetTerrainDensity(p + glm::vec3(eps, 0, 0)) - GetTerrainDensity(p - glm::vec3(eps, 0, 0));
		float dy = GetTerrainDensity(p + glm::vec3(0, eps, 0)) - GetTerrainDensity(p - glm::vec3(0, eps, 0));
		float dz = GetTerrainDensity(p + glm::vec3(0, 0, eps)) - GetTerrainDensity(p - glm::vec3(0, 0, eps));

		return glm::normalize(glm::vec3(dx, dy, dz));
	}

	void AddToGenerationQueue(std::shared_ptr<COctreeNode> pNode);
	void CheckApplyQueue();
	void GenerationWorkerLoop();

private:
	SBody *m_pBody = nullptr;
	CShader m_Shader;
	std::shared_ptr<COctreeNode> m_pRootNode;

	FastNoiseLite *m_pCaveNoise;
	FastNoiseLite *m_pMountainNoise;

	std::queue<std::shared_ptr<COctreeNode>> m_GenerationQueue;
	std::mutex m_GenQueueMutex;
	std::condition_variable m_GenQueueCV;

	std::queue<std::shared_ptr<COctreeNode>> m_ApplyQueue;
	std::mutex m_ApplyQueueMutex;
	std::condition_variable m_ApplyQueueCV;
	const size_t m_MaxApplyQueueSize = 100;

	std::vector<std::thread> m_vWorkerThreads;
	std::atomic<bool> m_bRunWorker;
};

class COctreeNode : public std::enable_shared_from_this<COctreeNode>
{
public:
	static const int VOXEL_RESOLUTION = 2;
	static const int MAX_LOD_LEVEL = 25;

	COctreeNode(CProceduralMesh *pOwnerMesh, std::weak_ptr<COctreeNode> pParent, glm::vec3 center, float size, int level);
	~COctreeNode();

	void Update(CCamera &Camera);
	void Render(CShader &Shader, const Vec3 &CameraAbsolutePos, const Vec3 &PlanetAbsolutePos);
	void GenerateMesh();
	void ApplyMeshBuffers();

private:
	void Subdivide();
	void Merge();

	CProceduralMesh *m_pOwnerMesh;
	std::weak_ptr<COctreeNode> m_pParent;
	std::shared_ptr<COctreeNode> m_pChildren[8];

	bool m_bIsLeaf = true;
	int m_Level;

	glm::vec3 m_Center;
	float m_Size;

	GLuint m_VAO = 0, m_VBO = 0, m_EBO = 0;
	unsigned int m_NumIndices = 0;

	std::atomic<bool> m_bIsGenerating;
	std::atomic<bool> m_bHasGeneratedData;
	std::atomic<bool> m_bGenerationAttempted;

	std::vector<SProceduralVertex> m_vGeneratedVertices;
	std::vector<unsigned int> m_vGeneratedIndices;
};

#endif // PROCEDURALMESH_H
