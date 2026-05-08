#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

class LightComponent;

struct VoxelMeshData {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int textureId;
};

class VoxelRenderer {
public:
    static VoxelRenderer& Get();

    void Init();

    void UpdateChunk(int cx, int cz, const std::vector<VoxelMeshData>& meshes);
    void RemoveChunk(int cx, int cz);
    void Clear();

    void SetHighlight(const glm::vec3& pos, bool active, float blockHalfSize);

    void RenderChunks(const glm::mat4& view, const glm::mat4& projection, LightComponent* light);
    void RenderChunksDepth(unsigned int depthProgram, const glm::mat4& lightVP);

private:
    VoxelRenderer() = default;
    ~VoxelRenderer();

    unsigned int getOrCreateChunkShader();
    unsigned int getDummyShadow();

    struct ChunkKey {
        int cx, cz;
        bool operator==(const ChunkKey& o) const { return cx == o.cx && cz == o.cz; }
    };
    struct ChunkKeyHash {
        size_t operator()(const ChunkKey& c) const {
            return std::hash<long long>()(((long long)c.cx << 32) | (unsigned int)c.cz);
        }
    };

    struct MeshGroup {
        unsigned int VAO = 0;
        unsigned int VBO = 0;
        unsigned int EBO = 0;
        unsigned int numIndices = 0;
        unsigned int textureId = 0;
    };

    struct ChunkRenderData {
        std::vector<MeshGroup> meshGroups;
    };

    void freeChunkMeshes(ChunkRenderData& chunk);

    std::unordered_map<ChunkKey, ChunkRenderData, ChunkKeyHash> m_chunks;

    bool m_highlightActive = false;
    glm::vec3 m_highlightPos = glm::vec3(0.0f);
    float m_blockHalfSize = 0.5f;
};
