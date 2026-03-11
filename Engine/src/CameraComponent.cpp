#include "CameraComponent.h"
#include "Scene.h"
#include "object.h"
#include <glm/gtc/matrix_transform.hpp>

glm::mat4 CameraComponent::GetViewMatrix() const {
    Vector3 pos = object->GetPosition3D();
    Vector3 ang = object->GetAngle();
    glm::mat4 view = glm::mat4(1.0f);

    view = glm::rotate(view, glm::radians(ang.x), glm::vec3(1,0,0));
    view = glm::rotate(view, glm::radians(ang.y), glm::vec3(0,1,0));
    view = glm::rotate(view, glm::radians(ang.z), glm::vec3(0,0,1));
    view = glm::translate(view, glm::vec3(-pos.x, -pos.y, -pos.z));
    return view;
}

glm::mat4 CameraComponent::GetProjectionMatrix() const {
    return glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
}

CameraComponent* CameraComponent::FindActive(Scene* scene) {
    if (!scene) return nullptr;
    const auto& objects = scene->GetObjects();
    for (auto* obj : objects) {
        if (!obj) continue;
        auto* cam = obj->GetComponent<CameraComponent>();
        if (cam && cam->IsActive()) return cam;
    }
    return nullptr;
}


