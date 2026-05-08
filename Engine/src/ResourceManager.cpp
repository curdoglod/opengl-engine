#include "ResourceManager.h"
#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <glm/glm.hpp>

ResourceManager& ResourceManager::Get() {
    static ResourceManager instance;
    return instance;
}

GLuint ResourceManager::LoadTexture(const std::string& path) {
    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end()) return it->second;

    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        std::cerr << "ResourceManager: cannot load '" << path
                  << "': " << IMG_GetError() << std::endl;
        return 0;
    }
    GLuint id = uploadSurface(surface);
    SDL_FreeSurface(surface);
    m_textureCache[path] = id;
    return id;
}

GLuint ResourceManager::LoadTextureFromMemory(const std::string& key,
                                               const std::vector<unsigned char>& data) {
    auto it = m_textureCache.find(key);
    if (it != m_textureCache.end()) return it->second;

    SDL_RWops* rw = SDL_RWFromConstMem(data.data(), (int)data.size());
    if (!rw) return 0;
    SDL_Surface* surface = IMG_Load_RW(rw, 1);
    if (!surface) {
        std::cerr << "ResourceManager: cannot load from memory '" << key
                  << "': " << SDL_GetError() << std::endl;
        return 0;
    }
    GLuint id = uploadSurface(surface);
    SDL_FreeSurface(surface);
    m_textureCache[key] = id;
    return id;
}

GLuint ResourceManager::uploadSurface(SDL_Surface* surface) {
    SDL_Surface* conv = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    if (!conv) return 0;

    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 conv->w, conv->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, conv->pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    SDL_FreeSurface(conv);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}

GLuint ResourceManager::GetOrCreateShader(const std::string& name,
                                           const char* vertSrc,
                                           const char* fragSrc) {
    auto it = m_shaderCache.find(name);
    if (it != m_shaderCache.end()) return it->second;

    GLuint program = compileProgram(vertSrc, fragSrc);
    m_shaderCache[name] = program;
    return program;
}

static GLuint compileShader_impl(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "ResourceManager shader compile error: " << log << std::endl;
    }
    return shader;
}

GLuint ResourceManager::compileProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vs   = compileShader_impl(GL_VERTEX_SHADER, vertSrc);
    GLuint fs   = compileShader_impl(GL_FRAGMENT_SHADER, fragSrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "ResourceManager shader link error: " << log << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static void processMeshIntoShared(aiMesh* mesh, const aiScene* scene, const std::string& directory, SharedMeshData& out) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;

    bool hasTex    = mesh->HasTextureCoords(0);
    bool hasNormals = mesh->HasNormals();

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        float px = mesh->mVertices[i].x;
        float py = mesh->mVertices[i].y;
        float pz = mesh->mVertices[i].z;
        vertices.push_back(px);
        vertices.push_back(py);
        vertices.push_back(pz);

        out.aabbMin.x = std::min(out.aabbMin.x, px);
        out.aabbMin.y = std::min(out.aabbMin.y, py);
        out.aabbMin.z = std::min(out.aabbMin.z, pz);
        out.aabbMax.x = std::max(out.aabbMax.x, px);
        out.aabbMax.y = std::max(out.aabbMax.y, py);
        out.aabbMax.z = std::max(out.aabbMax.z, pz);

        if (hasNormals) {
            vertices.push_back(mesh->mNormals[i].x);
            vertices.push_back(mesh->mNormals[i].y);
            vertices.push_back(mesh->mNormals[i].z);
        } else {
            vertices.push_back(0.0f); vertices.push_back(0.0f); vertices.push_back(1.0f);
        }
        if (hasTex) {
            vertices.push_back(mesh->mTextureCoords[0][i].x);
            vertices.push_back(mesh->mTextureCoords[0][i].y);
        } else {
            vertices.push_back(0.0f); vertices.push_back(0.0f);
        }
    }
    for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
        for (unsigned int j = 0; j < mesh->mFaces[f].mNumIndices; j++)
            indices.push_back(mesh->mFaces[f].mIndices[j]);
    }

    SharedMeshEntry entry;
    glGenVertexArrays(1, &entry.VAO);
    glGenBuffers(1, &entry.VBO);
    glGenBuffers(1, &entry.EBO);

    glBindVertexArray(entry.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, entry.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    int stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    entry.numIndices = (unsigned int)indices.size();

    if (mesh->mMaterialIndex < scene->mNumMaterials) {
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
            aiString texPath;
            mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
            std::string fullPath = directory + "/" + std::string(texPath.C_Str());
            entry.diffuseTexture = ResourceManager::Get().LoadTexture(fullPath);
        }
    }

    out.meshes.push_back(entry);
}

static void processNodeIntoShared(aiNode* node, const aiScene* scene, const std::string& directory, SharedMeshData& out) {
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
        processMeshIntoShared(scene->mMeshes[node->mMeshes[i]], scene, directory, out);
    for (unsigned int i = 0; i < node->mNumChildren; i++)
        processNodeIntoShared(node->mChildren[i], scene, directory, out);
}

const SharedMeshData* ResourceManager::GetOrLoadMesh(const std::string& path) {
    auto it = m_meshCache.find(path);
    if (it != m_meshCache.end()) return &it->second;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs);

    if (!scene || !scene->mRootNode || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) {
        std::cerr << "ResourceManager: Assimp failed to load '" << path
                  << "': " << importer.GetErrorString() << std::endl;
        return nullptr;
    }

    SharedMeshData& data = m_meshCache[path];
    std::string directory = path.substr(0, path.find_last_of('/'));
    processNodeIntoShared(scene->mRootNode, scene, directory, data);
    return &data;
}

void ResourceManager::ReleaseAll() {
    for (auto& [key, id] : m_textureCache)
        glDeleteTextures(1, &id);
    m_textureCache.clear();

    for (auto& [name, prog] : m_shaderCache)
        glDeleteProgram(prog);
    m_shaderCache.clear();

    for (auto& [path, data] : m_meshCache) {
        for (auto& mesh : data.meshes) {
            glDeleteVertexArrays(1, &mesh.VAO);
            glDeleteBuffers(1, &mesh.VBO);
            glDeleteBuffers(1, &mesh.EBO);
        }
    }
    m_meshCache.clear();
}
