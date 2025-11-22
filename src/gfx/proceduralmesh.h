#ifndef PROCEDURALMESH_H
#define PROCEDURALMESH_H

#include <GL/glew.h>
#include <array>
#include <atomic>
#include <condition_variable>
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
	glm::vec4 color_data;
};

class COctreeNode;

class CProceduralMesh
{
public:
	friend class COctreeNode;

	CProceduralMesh();
	~CProceduralMesh();

	void Init(SBody *pBody, EBodyType bodyType, int voxelResolution);
	void Update(CCamera &Camera);

	void Render(const CCamera &Camera, const SBody *pLightBody, bool bIsShadowPass);
	void RenderDebug(const CCamera &Camera);

	void Destroy();

	void AddToGenerationQueue(std::shared_ptr<COctreeNode> pNode, double distToCam);
	void CheckApplyQueue();
	void GenerationWorkerLoop();

	EBodyType m_BodyType;

	static const int VOXEL_RESOLUTION_DEFAULT = 16;

	// Planes are in Camera-Relative Space because View Matrix is rotation-only relative to 0,0,0
	std::array<glm::vec4, 6> m_FrustumPlanes;
	void CalculateFrustum(const CCamera &Camera);

	// Tunable LOD settings
	float m_SplitMultiplier = 0.2f;
	float m_MergeMultiplier = 0.1f;
	bool m_bVisualizeOctree = false;

	SBody *m_pBody = nullptr;
	CShader m_Shader;
	std::shared_ptr<COctreeNode> m_pRootNode;

	CTerrainGenerator m_TerrainGenerator;

	// Priority Queue Task
	struct SGenTask
	{
		std::shared_ptr<COctreeNode> m_pNode;
		double m_Priority; // Distance to camera

		// we want smallest distance at top, so operator< returns true if lhs has HIGHER distance
		bool operator<(const SGenTask &other) const
		{
			return m_Priority > other.m_Priority;
		}
	};

	std::priority_queue<SGenTask> m_GenerationQueue;
	std::mutex m_GenQueueMutex;
	std::condition_variable m_GenQueueCV;

	std::queue<std::shared_ptr<COctreeNode>> m_ApplyQueue;
	std::mutex m_ApplyQueueMutex;
	std::condition_variable m_ApplyQueueCV;
	const size_t m_MaxApplyQueueSize = 100;

	std::vector<std::thread> m_vWorkerThreads;
	std::atomic<bool> m_bRunWorker;

private:
	void InitDebug();
	CShader m_DebugShader;
	GLuint m_DebugCubeVAO = 0, m_DebugCubeVBO = 0, m_DebugCubeEBO = 0;
};

class COctreeNode : public std::enable_shared_from_this<COctreeNode>
{
public:
	static const int MAX_LOD_LEVEL = 50;

	COctreeNode(CProceduralMesh *pOwnerMesh, std::weak_ptr<COctreeNode> pParent, Vec3 center, double size, int level, int voxelResolution);
	~COctreeNode();

	void Update(CCamera &Camera);
	void Render(CShader &Shader, const Vec3 &CameraAbsolutePos, const Vec3 &PlanetAbsolutePos, const glm::mat4 &RotationMat);
	void GenerateMesh();
	void ApplyMeshBuffers();

	// Friend for debug rendering
	friend class CProceduralMesh;

private:
	void Subdivide();
	void Merge();

	CProceduralMesh *m_pOwnerMesh;
	std::weak_ptr<COctreeNode> m_pParent;
	std::shared_ptr<COctreeNode> m_pChildren[8];

	bool m_bIsLeaf = true;
	int m_Level;
	int m_VoxelResolution;

	Vec3 m_Center;
	double m_Size;

	GLuint m_VAO = 0, m_VBO = 0, m_EBO = 0;
	unsigned int m_NumIndices = 0;

	std::atomic<bool> m_bIsGenerating;
	std::atomic<bool> m_bHasGeneratedData;
	std::atomic<bool> m_bGenerationAttempted;

	std::vector<SProceduralVertex> m_vGeneratedVertices;
	std::vector<unsigned int> m_vGeneratedIndices;
};

#endif // PROCEDURALMESH_H
