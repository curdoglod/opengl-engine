#include "PlayerController.h"
#include "HotbarComponent.h"
#include "Scene.h"

void PlayerController::SetHotbar(HotbarComponent *hb)
{
    hotbar = hb;
}

void PlayerController::Update(float dt)
{
    if (!object)
        return;

    auto &input = InputManager::Get();

    Vector2 mouseDelta = input.GetMouseDelta();
    yaw += mouseDelta.x * mouseSensitivity;
    pitch += mouseDelta.y * mouseSensitivity;
    pitch = std::max(-89.0f, std::min(89.0f, pitch));

    Vector3 pos = object->GetPosition3D();
    WorldGridComponent *grid = findGrid();

    float vertical = (input.IsKeyDown(SDLK_w) ? 1.0f : 0.0f) + (input.IsKeyDown(SDLK_s) ? -1.0f : 0.0f);
    float horizontal = (input.IsKeyDown(SDLK_d) ? 1.0f : 0.0f) + (input.IsKeyDown(SDLK_a) ? -1.0f : 0.0f);

    if (vertical != 0.0f || horizontal != 0.0f)
    {
        const float toRad = 3.1415926535f / 180.0f;
        float cy = cosf(yaw * toRad);
        float sy = sinf(yaw * toRad);

        Vector3 forward(sy, 0.0f, -cy);
        Vector3 right(cy, 0.0f, sy);

        Vector3 move(
            forward.x * vertical + right.x * horizontal,
            0.0f,
            forward.z * vertical + right.z * horizontal);
        float len = std::sqrt(move.x * move.x + move.z * move.z);
        if (len > 0.0001f)
        {
            move.x /= len;
            move.z /= len;

            // Move per-axis so walls still allow sliding.
            float dx = move.x * moveSpeed * dt;
            float dz = move.z * moveSpeed * dt;
            moveHorizontal(grid, pos, dx, dz);
        }
    }

    if (input.IsKeyDown(SDLK_SPACE) && isGrounded)
    {
        velocityY = jumpSpeed;
        isGrounded = false;
    }

    for (int i = 0; i < 9; ++i)
    {
        if (input.IsKeyDown(SDLK_1 + i))
        {
            if (hotbar)
                hotbar->SetSelectedSlot(i);
            break;
        }
    }

    moveVertical(grid, pos, dt);

    object->SetPosition(pos);

    if (cameraObject)
    {
        cameraObject->SetPosition(Vector3(pos.x, pos.y + eyeHeight, pos.z));
        cameraObject->SetRotation(Vector3(pitch, yaw, 0.0f));
    }

    updateHoveredBlock(grid, dt);
}

void PlayerController::OnMouseButtonDown(Vector2)
{
    if (!object || !object->GetScene())
        return;

    auto &clickInput = InputManager::Get();
    bool isLeftClick = clickInput.IsMouseButtonDown(SDL_BUTTON_LEFT);
    bool isRightClick = clickInput.IsMouseButtonDown(SDL_BUTTON_RIGHT);

    WorldGridComponent *grid = findGrid();
    if (!grid)
        return;

    hoverRayTimer = kHoverRayInterval;
    updateHoveredBlock(grid, 0.0f);

    if (!rayHitValid)
        return;

    if (isLeftClick)
    {
        if (grid->HasBlock(rayHitGx, rayHitGy, rayHitGz))
        {
            grid->RemoveBlockAt(rayHitGx, rayHitGy, rayHitGz);
            rayHitValid = false;
        }
    }
    else if (isRightClick)
    {
        if (rayPlaceValid && hotbar)
        {
            grid->CreateBlockAt(rayPlaceGx, rayPlaceGy, rayPlaceGz, hotbar->GetSelectedSlot());
            rayHitValid = false;
        }
    }
}
