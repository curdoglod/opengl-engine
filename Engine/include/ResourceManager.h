#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <GL/glew.h>
#include <glm/glm.hpp>

struct SDL_Surface;

// Shared mesh data — geometry owned by ResourceManager, shared across all
// Model3DComponent instances that load the same file.  This avoids parsing
// the FBX / OBJ with Assimp once per block in a voxel world.
struct SharedMeshEntry {
    GLuint VAO        = 0;
    GLuint VBO        = 0;
    GLuint EBO        = 0;
    unsigned int numIndices = 0;
    GLuint diffuseTexture = 0; // material diffuse texture (0 = none)
};

struct SharedMeshData {
    std::vector<SharedMeshEntry> meshes;
    glm::vec3 aabbMin = glm::vec3( 1e9f);
    glm::vec3 aabbMax = glm::vec3(-1e9f);
};

/// All resources are indexed by a string key:
///   - textures from disk:   the full file path
///   - textures from memory: a user-supplied key (e.g. archive-relative path)
///   - shader programs:      a human-readable name (e.g. "sprite", "model3d")
///
/// The manager owns every GL object and keeps them alive until ReleaseAll()
/// is called (just before the GL context is destroyed).
class ResourceManager {
public:
    static ResourceManager& Get();

    // Non-copyable
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;



    /// Load a 3-D model from a file path.  Returns shared (cached) geometry so
    /// every component that uses the same file shares the same VAO/VBO/EBO.
    /// Returns nullptr on failure.
    const SharedMeshData* GetOrLoadMesh(const std::string& path);


    /// Load a texture from a file path.  Returns the cached GLuint on repeat calls.
    GLuint LoadTexture(const std::string& path);

    /// Load a texture from in-memory image bytes.
    /// 'key' is a stable cache identifier (e.g. the archive-relative path).
    GLuint LoadTextureFromMemory(const std::string& key,
                                  const std::vector<unsigned char>& data);


    /// Return a compiled + linked shader program by name.
    /// If not yet compiled, builds it from the provided GLSL sources.
    GLuint GetOrCreateShader(const std::string& name,
                              const char* vertSrc,
                              const char* fragSrc);

    void ReleaseAll();

private:
    ResourceManager() = default;

    GLuint uploadSurface(SDL_Surface* surface);
    static GLuint compileProgram(const char* vertSrc, const char* fragSrc);

    std::unordered_map<std::string, GLuint> m_textureCache;
    std::unordered_map<std::string, GLuint> m_shaderCache;
    std::unordered_map<std::string, SharedMeshData> m_meshCache;
};

#endif // RESOURCE_MANAGER_H
