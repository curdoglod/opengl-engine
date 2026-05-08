#include "Model3DComponent.h"
#include "object.h"
#include "ResourceManager.h"
#include "LightComponent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <SDL.h>
#include <iostream>

static const char* vertexShaderSource = R"(
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 Normal;     
out vec3 FragPos;
out vec4 LightSpacePos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightVP;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position   = projection * view * worldPos;

    FragPos  = worldPos.xyz;
    TexCoord = aTexCoord;

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    Normal = normalize(normalMatrix * aNormal);

    LightSpacePos = lightVP * worldPos;
}
)";

static const char* fragmentShaderSource = R"(
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
uniform vec4 highlightTint;

float ShadowCalculation(vec4 lightSpacePos, vec3 normal, vec3 lightDirection)
{
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    float currentDepth = projCoords.z;

    // Bias keeps lit faces from shadowing themselves.
    float cosTheta = max(dot(normalize(normal), -normalize(lightDirection)), 0.0);
    float bias = mix(0.002, 0.0004, cosTheta);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    int samples = 0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
            samples++;
        }
    }
    shadow /= float(samples);

    // Fade at the shadow-map edge so the cutoff is less obvious.
    float fadeRange = 0.05;
    float fadeFactor = 1.0;
    fadeFactor *= smoothstep(0.0, fadeRange, projCoords.x);
    fadeFactor *= smoothstep(0.0, fadeRange, 1.0 - projCoords.x);
    fadeFactor *= smoothstep(0.0, fadeRange, projCoords.y);
    fadeFactor *= smoothstep(0.0, fadeRange, 1.0 - projCoords.y);
    shadow *= fadeFactor;

    return shadow;
}

void main()
{
    vec3 texColor = texture(ourTexture, TexCoord).rgb;
    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, -lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 ambient = ambientColor;

    float shadow = 0.0;
    if (useShadows == 1) {
        shadow = ShadowCalculation(LightSpacePos, norm, lightDir);
    }

    vec3 result = texColor * (ambient + (1.0 - shadow) * diffuse);
    result = mix(result, highlightTint.rgb, highlightTint.a);
    FragColor   = vec4(result, 1.0);
}
)";

Model3DComponent::Model3DComponent(const std::string& modelPath)
    : modelPath(modelPath)
{
}

Model3DComponent::~Model3DComponent()
{
}

void Model3DComponent::Init()
{
    sharedMesh = ResourceManager::Get().GetOrLoadMesh(modelPath);
    if (!sharedMesh) {
        std::cerr << "Failed to load model: " << modelPath << std::endl;
        return;
    }
    aabbMin = sharedMesh->aabbMin;
    aabbMax = sharedMesh->aabbMax;
    aabbComputed = true;
    modelDims = aabbMax - aabbMin;

    if (object && object->GetSize3D().x == 0 && object->GetSize3D().y == 0 && object->GetSize3D().z == 0) {
        object->SetSize(Vector3(modelDims.x, modelDims.y, modelDims.z));
    }
}

glm::mat4 Model3DComponent::ComputeModelMatrix() const
{
    Vector3 p = object->GetPosition3D();
    glm::vec3 position(p.x, p.y, p.z);
    Vector3 angle = object->GetAngle();

    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 local = glm::mat4(1.0f);

    Vector3 targetSize = object->GetSize3D();
    if (sizeIsRelative && aabbComputed) {
        glm::vec3 dims = (modelDims == glm::vec3(0.0f)) ? (aabbMax - aabbMin) : modelDims;
        targetSize = Vector3(
            dims.x * (targetSize.x == 0 ? 1.0f : targetSize.x),
            dims.y * (targetSize.y == 0 ? 1.0f : targetSize.y),
            dims.z * (targetSize.z == 0 ? 1.0f : targetSize.z)
        );
    }
    if (aabbComputed) {
        glm::vec3 dims = aabbMax - aabbMin;
        glm::vec3 center = (aabbMin + aabbMax) * 0.5f;
        float sx = dims.x != 0.0f ? targetSize.x / dims.x : 1.0f;
        float sy = dims.y != 0.0f ? targetSize.y / dims.y : 1.0f;
        float sz = dims.z != 0.0f ? targetSize.z / dims.z : 1.0f;
        local = glm::scale(local, glm::vec3(sx, sy, sz));
        local = glm::translate(local, -center);
    } else {
        local = glm::scale(local, glm::vec3(targetSize.x, targetSize.y, targetSize.z));
    }

    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(angle.x), glm::vec3(1, 0, 0));
    model = glm::rotate(model, glm::radians(angle.y), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(angle.z), glm::vec3(0, 0, 1));
    model = model * local;
    return model;
}

void Model3DComponent::RenderDepthPass(const glm::mat4& model, GLuint depthProgram) const
{
    if (!sharedMesh) return;
    
    static std::unordered_map<GLuint, GLint> depthModelLocCache;
    GLint modelLoc;
    auto it = depthModelLocCache.find(depthProgram);
    if (it == depthModelLocCache.end()) {
        modelLoc = glGetUniformLocation(depthProgram, "model");
        depthModelLocCache[depthProgram] = modelLoc;
    } else {
        modelLoc = it->second;
    }
    
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    for (const auto& mesh : sharedMesh->meshes) {
        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

bool Model3DComponent::SetAlbedoTextureFromFile(const std::string& fullPath)
{
    GLuint id = ResourceManager::Get().LoadTexture(fullPath);
    if (id == 0) return false;
    overrideAlbedoTexture = id;
    return true;
}

// Fallback texture for frames without a real shadow map.
// GL_RED avoids incomplete-depth-texture issues on macOS.
static GLuint getDummyShadowMap() {
    static GLuint dummy = 0;
    if (dummy == 0) {
        glGenTextures(1, &dummy);
        glBindTexture(GL_TEXTURE_2D, dummy);
        unsigned char white = 255;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    return dummy;
}

void Model3DComponent::Render(const glm::mat4& view, const glm::mat4& projection, LightComponent* light)
{
    glm::mat4 model = ComputeModelMatrix();

    GLuint prog = ResourceManager::Get().GetOrCreateShader("model3d", vertexShaderSource, fragmentShaderSource);
    glUseProgram(prog);

    struct Uniforms {
        GLint model, view, proj, shadowMap, lightDir, lightColor, ambientColor, lightVP, useShadows, highlightTint, ourTexture;
    };
    static std::unordered_map<GLuint, Uniforms> uniformCache;
    
    auto it = uniformCache.find(prog);
    if (it == uniformCache.end()) {
        Uniforms u;
        u.model = glGetUniformLocation(prog, "model");
        u.view  = glGetUniformLocation(prog, "view");
        u.proj  = glGetUniformLocation(prog, "projection");
        u.shadowMap = glGetUniformLocation(prog, "shadowMap");
        u.lightDir = glGetUniformLocation(prog, "lightDir");
        u.lightColor = glGetUniformLocation(prog, "lightColor");
        u.ambientColor = glGetUniformLocation(prog, "ambientColor");
        u.lightVP = glGetUniformLocation(prog, "lightVP");
        u.useShadows = glGetUniformLocation(prog, "useShadows");
        u.highlightTint = glGetUniformLocation(prog, "highlightTint");
        u.ourTexture = glGetUniformLocation(prog, "ourTexture");
        uniformCache[prog] = u;
        it = uniformCache.find(prog);
    }
    const Uniforms& u = it->second;

    glUniformMatrix4fv(u.model, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(u.view,  1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(u.proj,  1, GL_FALSE, glm::value_ptr(projection));

    glm::vec3 lightDir(0.0f, 0.0f, -1.0f);
    glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
    glm::vec3 ambientColor(0.2f, 0.2f, 0.2f);
    glm::mat4 lightVP(1.0f);
    int useShadows = 0;

    if (light) {
        lightDir = light->GetDirection();
        lightColor = light->GetColor();
        ambientColor = light->GetAmbient();
        lightVP = light->GetLightVP();
        useShadows = light->IsShadowEnabled() && (light->GetDepthTexture() != 0) ? 1 : 0;
    }

    // Keep the sampler bound even when shadows are off.
    GLuint shadowTex = (light && light->GetDepthTexture() != 0)
                       ? light->GetDepthTexture()
                       : getDummyShadowMap();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    glUniform1i(u.shadowMap, 1);

    glUniform3fv(u.lightDir, 1, glm::value_ptr(lightDir));
    glUniform3fv(u.lightColor, 1, glm::value_ptr(lightColor));
    glUniform3fv(u.ambientColor, 1, glm::value_ptr(ambientColor));
    glUniformMatrix4fv(u.lightVP, 1, GL_FALSE, glm::value_ptr(lightVP));
    glUniform1i(u.useShadows, useShadows);
    glUniform4fv(u.highlightTint, 1, glm::value_ptr(highlightTint));

    if (!sharedMesh) { glUseProgram(0); return; }
    for (const auto& mesh : sharedMesh->meshes) {
        GLuint albedoTex = overrideAlbedoTexture ? overrideAlbedoTexture : mesh.diffuseTexture;
        if (albedoTex != 0) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, albedoTex);
            glUniform1i(u.ourTexture, 0);
        }
        glBindVertexArray(mesh.VAO);
        glDrawElements(GL_TRIANGLES, mesh.numIndices, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    glUseProgram(0);
}
