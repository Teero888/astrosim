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
	void Update(const CCamera &Camera);
	void Render(const CCamera &Camera, const SBody *pLightBody);
	void Destroy();

	float GetTerrainDensity(glm::vec3 worldPosition)
	{
		float PlanetRadius = (float)m_pBody->m_RenderParams.m_Radius;
		// TODO: make this work lol
		float Caves = m_pCaveNoise->GetNoise((worldPosition.x / PlanetRadius) * 100.f,
			(worldPosition.y / PlanetRadius) * 100.f,
			(worldPosition.z / PlanetRadius) * 100.f);
		float Base = PlanetRadius - glm::length(worldPosition);
		Base /= PlanetRadius;

		return Base - Caves;
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
	static const int VOXEL_RESOLUTION = 8;
	static const int MAX_LOD_LEVEL = 5;

	COctreeNode(CProceduralMesh *pOwnerMesh, std::weak_ptr<COctreeNode> pParent, glm::vec3 center, float size, int level);
	~COctreeNode();

	void Update(const CCamera &Camera);
	void Render(CShader &Shader);
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

	std::vector<SProceduralVertex> m_vGeneratedVertices;
	std::vector<unsigned int> m_vGeneratedIndices;
};

#endif // PROCEDURALMESH_H
