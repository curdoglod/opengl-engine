#include "RenderSystem.h"
#include "Scene.h"
#include "object.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "Model3DComponent.h"
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

    LightComponent *light = LightComponent::FindActive(scene);

    if (light && light->IsShadowEnabled())
    {
        light->RenderShadowMap(scene);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_DEPTH_TEST);

    const auto &objects = scene->GetObjects();
    for (auto *obj : objects)
    {
        if (!obj->IsActive())
            continue;

        auto *model = obj->GetComponent<Model3DComponent>();
        if (model)
        {
            glEnable(GL_DEPTH_TEST);
            model->Render(view, projection, light);
            continue;
        }

        auto *image = obj->GetComponent<Image>();
        if (image)
        {
            glDisable(GL_DEPTH_TEST);
            image->Render();
        }

        auto *text = obj->GetComponent<TextComponent>();
        if (text)
        {
            glDisable(GL_DEPTH_TEST);
            text->Render();
        }
    }

    glEnable(GL_DEPTH_TEST);
}
