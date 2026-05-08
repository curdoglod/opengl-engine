#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <GL/glew.h>
#include <glm/glm.hpp>

struct SDL_Surface;

// Cached model geometry shared by all Model3DComponent instances.
struct SharedMeshEntry {
    GLuint VAO        = 0;
    GLuint VBO        = 0;
    GLuint EBO        = 0;
    unsigned int numIndices = 0;
    GLuint diffuseTexture = 0;
};

struct SharedMeshData {
    std::vector<SharedMeshEntry> meshes;
    glm::vec3 aabbMin = glm::vec3( 1e9f);
    glm::vec3 aabbMax = glm::vec3(-1e9f);
};

// Owns cached textures, shaders, and model meshes.
class ResourceManager {
public:
    static ResourceManager& Get();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    const SharedMeshData* GetOrLoadMesh(const std::string& path);

    GLuint LoadTexture(const std::string& path);

    GLuint LoadTextureFromMemory(const std::string& key,
                                  const std::vector<unsigned char>& data);

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
