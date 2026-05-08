#include "LightComponent.h"
#include "Renderer.h"
#include "Scene.h"
#include "object.h"
#include "Model3DComponent.h"
#include "CameraComponent.h"
#include "VoxelRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <iostream>
#include <cmath>

namespace {
GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok=0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok){ char log[512]; glGetShaderInfoLog(s,512,nullptr,log); std::cerr << "Light shader compile: "<<log<<std::endl; }
    return s;
}
GLuint link(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p,vs); glAttachShader(p,fs);
    glLinkProgram(p);
    GLint ok=0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok){ char log[512]; glGetProgramInfoLog(p,512,nullptr,log); std::cerr << "Light program link: "<<log<<std::endl; }
    glDeleteShader(vs); glDeleteShader(fs);
    return p;
}
}

static GLuint depthProgram = 0;

LightComponent::LightComponent()
    : direction(glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f)))
    , color(1.0f, 1.0f, 1.0f)
    , ambient(0.2f, 0.2f, 0.2f)
    , enableShadows(true)
    , shadowWidth(1024)
    , shadowHeight(1024)
    , depthFBO(0)
    , depthTexture(0)
    , lightView(1.0f)
    , lightProj(1.0f)
{}

LightComponent::~LightComponent()
{
    if (depthTexture) glDeleteTextures(1, &depthTexture);
    if (depthFBO) glDeleteFramebuffers(1, &depthFBO);
}

void LightComponent::Init()
{
    if (depthProgram == 0) {
        const char* vsSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 model;
uniform mat4 lightVP;
void main(){
    gl_Position = lightVP * model * vec4(aPos,1.0);
}
)";
        const char* fsSrc = R"(
#version 330 core
void main(){ }
)";
        depthProgram = link(compile(GL_VERTEX_SHADER, vsSrc), compile(GL_FRAGMENT_SHADER, fsSrc));
    }
    ensureShadowResources();
}

void LightComponent::ensureShadowResources()
{
    if (!enableShadows) return;
    if (depthFBO == 0) glGenFramebuffers(1, &depthFBO);
    if (depthTexture == 0) {
        glGenTextures(1, &depthTexture);
        glBindTexture(GL_TEXTURE_2D, depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowWidth, shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = {1.0,1.0,1.0,1.0};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow framebuffer not complete" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void LightComponent::computeLightMatrices(Scene* scene)
{
    // Keep the shadow box centered near the camera.
    glm::vec3 center(0.0f);
    const auto& objects = scene->GetObjects();
    for (auto* obj : objects) {
        auto* cam = obj->GetComponent<CameraComponent>();
        if (cam) {
            Vector3 p = obj->GetPosition3D();
            center = glm::vec3(p.x, p.y, p.z);
            break;
        }
    }

    shadowCenter = center;
    const float range = 25.0f;
    shadowRangeSq = range * range;

    glm::vec3 up = (std::abs(direction.y) > 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    glm::vec3 lightPos = center - direction * (range * 2.0f);
    lightView = glm::lookAt(lightPos, center, up);
    lightProj = glm::ortho(-range, range, -range, range, 0.1f, range * 4.0f);

    // Texel snapping keeps shadows from shimmering while the camera moves.
    glm::mat4 shadowMat = lightProj * lightView;
    glm::vec4 origin = shadowMat * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    origin.x *= (float)shadowWidth  * 0.5f;
    origin.y *= (float)shadowHeight * 0.5f;
    float rx = std::round(origin.x);
    float ry = std::round(origin.y);
    lightProj[3][0] += (rx - origin.x) * 2.0f / (float)shadowWidth;
    lightProj[3][1] += (ry - origin.y) * 2.0f / (float)shadowHeight;
}

void LightComponent::RenderShadowMap(Scene* scene)
{
    if (!enableShadows || !scene) return;
    ensureShadowResources();
    computeLightMatrices(scene);

    glViewport(0,0,shadowWidth,shadowHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(depthProgram);
    static std::unordered_map<GLuint, GLint> depthLightVPLocCache;
    GLint lightVPLoc;
    auto it = depthLightVPLocCache.find(depthProgram);
    if (it == depthLightVPLocCache.end()) {
        lightVPLoc = glGetUniformLocation(depthProgram, "lightVP");
        depthLightVPLocCache[depthProgram] = lightVPLoc;
    } else {
        lightVPLoc = it->second;
    }
    glm::mat4 lightVP = GetLightVP();
    glUniformMatrix4fv(lightVPLoc, 1, GL_FALSE, glm::value_ptr(lightVP));

    const auto& objects = scene->GetObjects();
    for (auto* obj : objects) {
        if (!obj->IsActive()) continue;
        auto* modelComp = obj->GetComponent<Model3DComponent>();
        if (!modelComp) continue;
        Vector3 p = obj->GetPosition3D();
        float dx = p.x - shadowCenter.x;
        float dz = p.z - shadowCenter.z;
        if (dx*dx + dz*dz > shadowRangeSq) continue;
        glm::mat4 model = modelComp->ComputeModelMatrix();
        modelComp->RenderDepthPass(model, depthProgram);
    }

    VoxelRenderer::Get().RenderChunksDepth(depthProgram, lightVP);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    glViewport(0, 0, Renderer::Get().GetWindowWidth(), Renderer::Get().GetWindowHeight());
}

LightComponent* LightComponent::FindActive(Scene* scene)
{
    if (!scene) return nullptr;
    const auto& objects = scene->GetObjects();
    for (auto* obj : objects) {
        auto* light = obj->GetComponent<LightComponent>();
        if (light) return light;
    }
    return nullptr;
}
