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

    if (grid)
    {
        pushOutOfBlocks(grid, pos);
    }

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

            // Try X and Z movement separately so the player can slide
            // along walls instead of getting stuck completely.
            float dx = move.x * moveSpeed * dt;
            float dz = move.z * moveSpeed * dt;

            if (grid)
            {
                // Try moving along X
                Vector3 testX(pos.x + dx, pos.y, pos.z);
                if (!isCollidingHorizontally(grid, testX))
                {
                    pos.x += dx;
                }
                // Try moving along Z
                Vector3 testZ(pos.x, pos.y, pos.z + dz);
                if (!isCollidingHorizontally(grid, testZ))
                {
                    pos.z += dz;
                }
            }
            else
            {
                pos.x += dx;
                pos.z += dz;
            }
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

    velocityY += gravity * dt;
    pos.y += velocityY * dt;

    if (grid)
    {
        float groundY = getGroundLevel(grid, pos);
        if (pos.y <= groundY)
        {
            pos.y = groundY;
            velocityY = 0.0f;
            isGrounded = true;
        }
        else
        {
            isGrounded = false;
        }

        // Stop upward velocity if the head collides with a block.
        float headY = pos.y + eyeHeight;
        int gx, gy, gz;
        if (grid->WorldToGrid(Vector3(pos.x, headY, pos.z), gx, gy, gz))
        {
            if (grid->GetBlock(gx, gy, gz))
            {
                Vector3 blockWorld = grid->GridToWorld(gx, gy, gz);
                float blockBottom = blockWorld.y - grid->GetBlockSize() * 0.5f;
                pos.y = blockBottom - eyeHeight;
                if (velocityY > 0.0f)
                    velocityY = 0.0f;
            }
        }
    }

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

    // Force immediate raycast refresh so click uses current crosshair target.
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
        if (rayHasEmpty && hotbar)
        {
            grid->CreateBlockAt(rayEmptyGx, rayEmptyGy, rayEmptyGz, hotbar->GetSelectedSlot());
            rayHitValid = false; // invalidate cache
        }
    }
}