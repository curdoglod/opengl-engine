#include "Rigidbody3D.h"
#include "BoxCollider3D.h"
#include "object.h"
#include "Scene.h"
#include <algorithm>
#include <cmath>
#include <iostream>

void Rigidbody3D::Update(float dt)
{
    if (!object) return;

    Vector3 frameAccel = acceleration;
    if (useGravity) {
        frameAccel.y += gravity;
    }

    velocity += frameAccel * dt;
    object->SetPosition(object->GetPosition3D() + velocity * dt);

    acceleration = Vector3(0, 0, 0);

    resolveCollisions();
}

void Rigidbody3D::integrate(float dt)
{
    velocity += acceleration * dt;
    object->SetPosition(object->GetPosition3D() + velocity * dt);
}

void Rigidbody3D::resolveCollisions()
{
    if (!object || !object->GetScene()) return;

    BoxCollider3D* myCol = object->GetComponent<BoxCollider3D>();
    if (!myCol) return;

    myCol->AutoFitFromModel();

    const auto& objects = object->GetScene()->GetObjects();
    for (auto* otherObj : objects) {
        if (otherObj == object) continue;
        if (!otherObj) continue;
        BoxCollider3D* otherCol = otherObj->GetComponent<BoxCollider3D>();
        if (!otherCol) continue;

        if (myCol->IsTrigger() || otherCol->IsTrigger()) continue;

        otherCol->AutoFitFromModel();

        if (myCol->Overlaps(otherCol)) {
            object->notifyCollisionEnter(otherObj);
            otherObj->notifyCollisionEnter(object);

            Vector3 c1 = myCol->GetCenter();
            Vector3 c2 = otherCol->GetCenter();
            Vector3 e1 = myCol->GetHalfExtents();
            Vector3 e2 = otherCol->GetHalfExtents();

            float dx = (e1.x + e2.x) - std::abs(c1.x - c2.x);
            float dy = (e1.y + e2.y) - std::abs(c1.y - c2.y);
            float dz = (e1.z + e2.z) - std::abs(c1.z - c2.z);


            if (dx <= dy && dx <= dz) {
                float sx = (c1.x < c2.x) ? -dx : dx;
                object->SetPosition(object->GetPosition3D() + Vector3(sx, 0, 0));
                velocity.x = 0;
            } else if (dy <= dx && dy <= dz) {
                float sy = (c1.y < c2.y) ? -dy : dy;
                object->SetPosition(object->GetPosition3D() + Vector3(0, sy, 0));
                velocity.y = 0;
            } else {
                float sz = (c1.z < c2.z) ? -dz : dz;
                object->SetPosition(object->GetPosition3D() + Vector3(0, 0, sz));
                velocity.z = 0;
            }
        }
    }
}
