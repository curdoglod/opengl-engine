#include "BoxCollider3D.h"
#include "Model3DComponent.h"
#include "object.h"
#include <cmath>
#include <iostream>

void BoxCollider3D::Init()
{
    AutoFitFromModel();
}

void BoxCollider3D::AutoFitFromModel()
{
    if (!object) return;
    Model3DComponent* model = object->GetComponent<Model3DComponent>();

    Vector3 size = object->GetSize3D();

    Vector3 dimsUsed;
    bool hasModel = (model != nullptr);
    bool hasAabb = hasModel ? model->HasAabb() : false;
    bool rel = hasModel ? model->GetSizeIsRelative() : false;

    if (hasModel && hasAabb) {
        glm::vec3 importDims = model->GetModelDims();
        if (rel) {
            float rx = (size.x == 0.0f) ? 1.0f : size.x;
            float ry = (size.y == 0.0f) ? 1.0f : size.y;
            float rz = (size.z == 0.0f) ? 1.0f : size.z;
            dimsUsed = Vector3(importDims.x * rx,
                               importDims.y * ry,
                               importDims.z * rz);
        } else {
            dimsUsed = Vector3(size.x == 0.0f ? importDims.x : size.x,
                               size.y == 0.0f ? importDims.y : size.y,
                               size.z == 0.0f ? importDims.z : size.z);
        }
    } else {
        dimsUsed = Vector3(size.x != 0.0f ? size.x : 1.0f,
                           size.y != 0.0f ? size.y : 1.0f,
                           size.z != 0.0f ? size.z : 1.0f);
    }

    halfExtents = dimsUsed * 0.5f;
}

bool BoxCollider3D::Overlaps(const BoxCollider3D* other) const
{
    if (!other) return false;
    Vector3 c1 = GetCenter();
    Vector3 c2 = other->GetCenter();
    Vector3 e1 = GetHalfExtents();
    Vector3 e2 = other->GetHalfExtents();

    bool overlapX = std::abs(c1.x - c2.x) <= (e1.x + e2.x);
    bool overlapY = std::abs(c1.y - c2.y) <= (e1.y + e2.y);
    bool overlapZ = std::abs(c1.z - c2.z) <= (e1.z + e2.z);

    return overlapX && overlapY && overlapZ;
}
