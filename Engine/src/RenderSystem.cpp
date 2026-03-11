#include "RenderSystem.h"
#include "Scene.h"
#include "object.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "Model3DComponent.h"
#include "IVoxelRenderer.h"
#include "Frustum.h"
#include "image.h"
#include "text.h"
#include "Renderer.h"
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void RenderSystem::Render(Scene *scene)
{
    if (!scene)
        return;

    CameraComponent *cam = CameraComponent::FindActive(scene);
    glm::mat4 view;
    glm::mat4 projection;
    if (cam)
    {
        view = cam->GetViewMatrix();
        projection = cam->GetProjectionMatrix();
    }
    else
    {
        view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 5.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
        projection = glm::perspective(
            glm::radians(60.0f),
            static_cast<float>(Renderer::Get().GetWindowWidth()) /
                static_cast<float>(Renderer::Get().GetWindowHeight()),
            0.1f, 100.0f);
    }

    Frustum frustum;
    frustum.Extract(projection * view);

    LightComponent *light = LightComponent::FindActive(scene);

    if (light && light->IsShadowEnabled())
    {
        light->RenderShadowMap(scene);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (IVoxelRenderer::s_instance)
    {
        glEnable(GL_DEPTH_TEST);
        IVoxelRenderer::s_instance->RenderChunks(view, projection, light, frustum);
    }

    const auto &objects = scene->GetObjects();
    for (auto *obj : objects)
    {
        if (!obj->IsActive())
            continue;

        auto *model = obj->GetComponent<Model3DComponent>();
        if (model)
        {
            Vector3 p = obj->GetPosition3D();
            glm::vec3 pos(p.x, p.y, p.z);
            float radius = 2.0f;
            if (model->HasAabb())
            {
                glm::vec3 dims = model->GetModelDims();
                radius = glm::length(dims) * 0.5f;
                if (radius < 0.5f) radius = 0.5f;
            }
            if (!frustum.TestSphere(pos, radius))
                continue;

            glEnable(GL_DEPTH_TEST);
            model->Render(view, projection, light);
            continue;
        }

        // 2D sprite — depth test OFF
        auto *image = obj->GetComponent<Image>();
        if (image)
        {
            glDisable(GL_DEPTH_TEST);
            image->Render();
        }

        // Text — depth test OFF
        auto *text = obj->GetComponent<TextComponent>();
        if (text)
        {
            glDisable(GL_DEPTH_TEST);
            text->Render();
        }
    }

    glEnable(GL_DEPTH_TEST);
}
