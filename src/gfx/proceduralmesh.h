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
#include "terrain/terrain.h"

struct SProceduralVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoord;
	glm::vec2 color_data;
};

class COctreeNode; // Forward declare

class CProceduralMesh
{
public:
	friend class COctreeNode;

	CProceduralMesh();
	~CProceduralMesh();

	void Init(SBody *pBody, EBodyType bodyType, int voxelResolution);
	void Update(CCamera &Camera);
	void Render(const CCamera &Camera, const SBody *pLightBody);
	void Destroy();

	void AddToGenerationQueue(std::shared_ptr<COctreeNode> pNode);
	void CheckApplyQueue();
	void GenerationWorkerLoop();

	EBodyType m_BodyType;
	static const int VOXEL_RESOLUTION_DEFAULT = 8;

private:
	SBody *m_pBody = nullptr;
	CShader m_Shader;
	std::shared_ptr<COctreeNode> m_pRootNode;

	CTerrainGenerator m_TerrainGenerator;

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
	static const int MAX_LOD_LEVEL = 25;

	COctreeNode(CProceduralMesh *pOwnerMesh, std::weak_ptr<COctreeNode> pParent, glm::vec3 center, float size, int level, int voxelResolution);
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
	int m_VoxelResolution;

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
