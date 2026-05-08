#include "VoxelRenderer.h"
#include "ResourceManager.h"
#include "LightComponent.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

VoxelRenderer& VoxelRenderer::Get() {
    static VoxelRenderer instance;
    return instance;
}

void VoxelRenderer::Init() {
}

VoxelRenderer::~VoxelRenderer() {
    Clear();
}

void VoxelRenderer::freeChunkMeshes(ChunkRenderData& chunk) {
    for (auto& mg : chunk.meshGroups) {
        if (mg.VAO) glDeleteVertexArrays(1, &mg.VAO);
        if (mg.VBO) glDeleteBuffers(1, &mg.VBO);
        if (mg.EBO) glDeleteBuffers(1, &mg.EBO);
    }
    chunk.meshGroups.clear();
}

void VoxelRenderer::UpdateChunk(int cx, int cz, const std::vector<VoxelMeshData>& meshes) {
    ChunkKey key{cx, cz};
    auto& chunk = m_chunks[key];
    freeChunkMeshes(chunk);

    for (const auto& meshData : meshes) {
        if (meshData.indices.empty()) continue;

        MeshGroup mg;
        mg.textureId = meshData.textureId;
        mg.numIndices = (unsigned int)meshData.indices.size();

        glGenVertexArrays(1, &mg.VAO);
        glGenBuffers(1, &mg.VBO);
        glGenBuffers(1, &mg.EBO);

        glBindVertexArray(mg.VAO);
        glBindBuffer(GL_ARRAY_BUFFER, mg.VBO);
        glBufferData(GL_ARRAY_BUFFER, meshData.vertices.size() * sizeof(float), meshData.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mg.EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData.indices.size() * sizeof(unsigned int), meshData.indices.data(), GL_STATIC_DRAW);

        int stride = 8 * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
        chunk.meshGroups.push_back(mg);
    }
}

void VoxelRenderer::RemoveChunk(int cx, int cz) {
    ChunkKey key{cx, cz};
    auto it = m_chunks.find(key);
    if (it != m_chunks.end()) {
        freeChunkMeshes(it->second);
        m_chunks.erase(it);
    }
}

void VoxelRenderer::Clear() {
    for (auto& kv : m_chunks) {
        freeChunkMeshes(kv.second);
    }
    m_chunks.clear();
}

void VoxelRenderer::SetHighlight(const glm::vec3& pos, bool active, float blockHalfSize) {
    m_highlightPos = pos;
    m_highlightActive = active;
    m_blockHalfSize = blockHalfSize;
}

unsigned int VoxelRenderer::getDummyShadow() {
    static GLuint dummy = 0;
    if (!dummy) {
        glGenTextures(1, &dummy);
        glBindTexture(GL_TEXTURE_2D, dummy);
        unsigned char w = 255;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &w);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    return dummy;
}

unsigned int VoxelRenderer::getOrCreateChunkShader() {
    static const char* vs = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aTexCoord;
layout(location=2) in vec3 aNormal;
out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 LightSpacePos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightVP;
void main(){
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;
    FragPos  = worldPos.xyz;
    TexCoord = aTexCoord;
    Normal   = aNormal;
    LightSpacePos = lightVP * worldPos;
})";
    static const char* fs = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;
in vec4 LightSpacePos;
uniform sampler2D ourTexture;
uniform sampler2D shadowMap;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform int useShadows;
uniform vec3 highlightPos;
uniform int highlightActive;
uniform float blockHalfSize;

float ShadowCalc(vec4 lsp, vec3 n, vec3 ld){
    vec3 p = lsp.xyz / lsp.w * 0.5 + 0.5;
    if(p.z>1.0||p.x<0.0||p.x>1.0||p.y<0.0||p.y>1.0) return 0.0;
    float cosT = max(dot(normalize(n), -normalize(ld)), 0.0);
    float bias = mix(0.002, 0.0004, cosT);
    float shadow = 0.0;
    vec2 ts = 1.0 / textureSize(shadowMap, 0);
    for(int x=-2;x<=2;++x) for(int y=-2;y<=2;++y){
        float d = texture(shadowMap, p.xy + vec2(x,y)*ts).r;
        shadow += (p.z - bias) > d ? 1.0 : 0.0;
    }
    shadow /= 25.0;
    float fr = 0.05;
    float f = smoothstep(0.0,fr,p.x)*smoothstep(0.0,fr,1.0-p.x)
             *smoothstep(0.0,fr,p.y)*smoothstep(0.0,fr,1.0-p.y);
    return shadow * f;
}
void main(){
    vec3 tex = texture(ourTexture, TexCoord).rgb;
    vec3 n = normalize(Normal);
    float diff = max(dot(n, -lightDir), 0.0);
    float shadow = useShadows==1 ? ShadowCalc(LightSpacePos, n, lightDir) : 0.0;
    vec3 result = tex * (ambientColor + (1.0-shadow)*diff*lightColor);
    if(highlightActive==1){
        vec3 d = abs(FragPos - highlightPos);
        if(d.x < blockHalfSize*1.01 && d.y < blockHalfSize*1.01 && d.z < blockHalfSize*1.01)
            result = mix(result, vec3(1.0,1.0,0.4), 0.18);
    }
    FragColor = vec4(result, 1.0);
})";
    return ResourceManager::Get().GetOrCreateShader("chunk_mesh", vs, fs);
}

void VoxelRenderer::RenderChunks(const glm::mat4& view, const glm::mat4& projection, LightComponent* light) {
    GLuint prog = getOrCreateChunkShader();
    glUseProgram(prog);

    struct Uniforms {
        GLint model, view, projection, lightDir, lightColor, ambientColor, lightVP, useShadows, shadowMap, highlightPos, highlightActive, blockHalfSize, ourTexture;
    };
    static std::unordered_map<GLuint, Uniforms> uniformCache;
    auto it = uniformCache.find(prog);
    if (it == uniformCache.end()) {
        Uniforms u;
        u.model = glGetUniformLocation(prog, "model");
        u.view = glGetUniformLocation(prog, "view");
        u.projection = glGetUniformLocation(prog, "projection");
        u.lightDir = glGetUniformLocation(prog, "lightDir");
        u.lightColor = glGetUniformLocation(prog, "lightColor");
        u.ambientColor = glGetUniformLocation(prog, "ambientColor");
        u.lightVP = glGetUniformLocation(prog, "lightVP");
        u.useShadows = glGetUniformLocation(prog, "useShadows");
        u.shadowMap = glGetUniformLocation(prog, "shadowMap");
        u.highlightPos = glGetUniformLocation(prog, "highlightPos");
        u.highlightActive = glGetUniformLocation(prog, "highlightActive");
        u.blockHalfSize = glGetUniformLocation(prog, "blockHalfSize");
        u.ourTexture = glGetUniformLocation(prog, "ourTexture");
        uniformCache[prog] = u;
        it = uniformCache.find(prog);
    }
    const Uniforms& u = it->second;

    glm::mat4 model(1.0f);
    glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(u.view, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u.projection, 1, GL_FALSE, glm::value_ptr(projection));

    glm::vec3 lightDir(0, 0, -1), lightColor(1), ambientColor(0.2f);
    glm::mat4 lightVP(1.0f);
    int useShadows = 0;
    if (light) {
        lightDir = light->GetDirection();
        lightColor = light->GetColor();
        ambientColor = light->GetAmbient();
        lightVP = light->GetLightVP();
        useShadows = (light->IsShadowEnabled() && light->GetDepthTexture()) ? 1 : 0;
    }
    glUniform3fv(u.lightDir, 1, glm::value_ptr(lightDir));
    glUniform3fv(u.lightColor, 1, glm::value_ptr(lightColor));
    glUniform3fv(u.ambientColor, 1, glm::value_ptr(ambientColor));
    glUniformMatrix4fv(u.lightVP, 1, GL_FALSE, glm::value_ptr(lightVP));
    glUniform1i(u.useShadows, useShadows);

    GLuint shadowTex = (light && light->GetDepthTexture()) ? light->GetDepthTexture() : getDummyShadow();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glUniform1i(u.shadowMap, 1);

    glUniform3fv(u.highlightPos, 1, glm::value_ptr(m_highlightPos));
    glUniform1i(u.highlightActive, m_highlightActive ? 1 : 0);
    glUniform1f(u.blockHalfSize, m_blockHalfSize);

    for (const auto& [cc, chunk] : m_chunks) {
        if (chunk.meshGroups.empty()) continue;

        for (const auto& mg : chunk.meshGroups) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mg.textureId);
            glUniform1i(u.ourTexture, 0);
            glBindVertexArray(mg.VAO);
            glDrawElements(GL_TRIANGLES, mg.numIndices, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
    }
    glUseProgram(0);
}

void VoxelRenderer::RenderChunksDepth(GLuint depthProgram, const glm::mat4& lightVP) {
    static std::unordered_map<GLuint, GLint> depthModelLocCache;
    GLint modelLoc;
    auto it = depthModelLocCache.find(depthProgram);
    if (it == depthModelLocCache.end()) {
        modelLoc = glGetUniformLocation(depthProgram, "model");
        depthModelLocCache[depthProgram] = modelLoc;
    } else {
        modelLoc = it->second;
    }

    glm::mat4 model(1.0f);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    for (const auto& [cc, chunk] : m_chunks) {
        if (chunk.meshGroups.empty()) continue;

        for (const auto& mg : chunk.meshGroups) {
            glBindVertexArray(mg.VAO);
            glDrawElements(GL_TRIANGLES, mg.numIndices, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
    }
}
