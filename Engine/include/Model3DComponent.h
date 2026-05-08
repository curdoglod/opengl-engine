#pragma once
#include "component.h"
#include "ResourceManager.h"
#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <limits>

class Model3DComponent : public Component
{
public:
    Model3DComponent(const std::string& modelPath);
    virtual ~Model3DComponent();

    virtual void Init() override;

    void SetSizeIsRelative(bool enabled) { sizeIsRelative = enabled; }
    bool GetSizeIsRelative() const { return sizeIsRelative; }

    bool HasAabb() const { return aabbComputed; }
    glm::vec3 GetModelDims() const { return modelDims; }

    glm::mat4 ComputeModelMatrix() const;
    void RenderDepthPass(const glm::mat4& model, GLuint depthProgram) const;

    void Render(const glm::mat4& view, const glm::mat4& projection, class LightComponent* light);

    void SetAlbedoTexture(GLuint textureId) { overrideAlbedoTexture = textureId; }
    bool SetAlbedoTextureFromFile(const std::string& fullPath);

    void SetHighlight(bool enabled,
                      glm::vec3 color     = glm::vec3(1.0f, 1.0f, 0.4f),
                      float     intensity = 0.04f)
    {
        highlightTint = enabled ? glm::vec4(color, intensity) : glm::vec4(0.0f);
    }

private:
    std::string modelPath;
    const SharedMeshData* sharedMesh = nullptr;

    glm::vec3 aabbMin = glm::vec3( std::numeric_limits<float>::max());
    glm::vec3 aabbMax = glm::vec3(-std::numeric_limits<float>::max());
    bool aabbComputed = false;
    glm::vec3 modelDims = glm::vec3(0.0f);
    bool sizeIsRelative = true;

    GLuint overrideAlbedoTexture = 0;
    glm::vec4 highlightTint = glm::vec4(0.0f);
};
