#pragma once
#include "component.h"
#include "object.h"
#include "Scene.h"
#include "Utils.h"
#include "BlockComponent.h"
#include "WorldGridComponent.h"
#include "CameraComponent.h"
#include "InputManager.h"
#include <SDL.h>
#include <cmath>
#include <algorithm>

class HotbarComponent;


class PlayerController : public Component
{
	static constexpr int kRaycastSteps = 20;

public:
	PlayerController()
		: moveSpeed(120.0f / 35.0f), cameraObject(nullptr), eyeHeight(30.0f / 35.0f), yaw(0.0f), pitch(0.0f), mouseSensitivity(0.15f), velocityY(0.0f), gravity(-600.0f / 35.0f), isGrounded(false), jumpSpeed(220.0f / 35.0f)
	{
	}

	void SetCamera(Object *cam) { cameraObject = cam; }
	void SetMoveSpeed(float s) { moveSpeed = s; }
	void SetMouseSensitivity(float s) { mouseSensitivity = s; }
	void SetEyeHeight(float h) { eyeHeight = h; }
	void SetJumpSpeed(float s) { jumpSpeed = s; }
	void SetGravity(float g) { gravity = g; }

	void Init() override
	{

	}

	void Update(float dt) override;


	void OnMouseButtonDown(Vector2) override;

	void OnMouseButtonMotion(Vector2 mouse_position) override
	{
	}

	void SetHotbar(HotbarComponent *hb);

private:

	Vector3 getLookDirection() const
	{
		const float toRad = 3.1415926535f / 180.0f;
		float cy = cosf(yaw * toRad);
		float sy = sinf(yaw * toRad);
		float cp = cosf(pitch * toRad);
		float sp = sinf(pitch * toRad);
		return Vector3(sy * cp, -sp, -cy * cp);
	}

	void updateHoveredBlock(WorldGridComponent *grid, float dt)
	{
		hoverRayTimer += dt;
		if (hoverRayTimer < kHoverRayInterval)
			return;
		hoverRayTimer = 0.0f;

		bool newHit = false;
		rayHitValid = false;
		rayHasEmpty = false;

		if (grid && cameraObject)
		{
			Vector3 rayStart = cameraObject->GetPosition3D();
			Vector3 rayDir = getLookDirection();
			float stepSize = grid->GetBlockSize() * 0.4f;
			Vector3 currentPos = rayStart;
			for (int i = 0; i < kRaycastSteps; ++i)
			{
				int gx, gy, gz;
				if (grid->WorldToGrid(currentPos, gx, gy, gz))
				{
					if (grid->HasBlock(gx, gy, gz))
					{
						newHit = true;
						rayHitValid = true;
						rayHitGx = gx; rayHitGy = gy; rayHitGz = gz;
						break;
					}
					else
					{
						rayHasEmpty = true;
						rayEmptyGx = gx; rayEmptyGy = gy; rayEmptyGz = gz;
					}
				}
				currentPos = currentPos + rayDir * stepSize;
			}
		}
		if (newHit)
			grid->SetHighlightBlock(rayHitGx, rayHitGy, rayHitGz);
		else if (grid)
			grid->ClearHighlight();
	}

	WorldGridComponent *findGrid()
	{
		if (cachedGrid)
			return cachedGrid;
		if (!object || !object->GetScene())
			return nullptr;
		const auto &objects = object->GetScene()->GetObjects();
		for (auto *obj : objects)
		{
			auto *g = obj->GetComponent<WorldGridComponent>();
			if (g)
			{
				cachedGrid = g;
				return g;
			}
		}
		return nullptr;
	}


	float getGroundLevel(WorldGridComponent *grid, const Vector3 &pos) const
	{
		int gx, gy, gz;
		if (!grid->WorldToGrid(pos, gx, gy, gz))
		{
			return 0.0f;
		}
		for (int y = gy; y >= 0; --y)
		{
			if (grid->GetBlock(gx, y, gz))
			{
				Vector3 blockWorld = grid->GridToWorld(gx, y, gz);
				return blockWorld.y + grid->GetBlockSize() * 0.5f;
			}
		}
		return 0.0f; // no ground found — fall to Y=0
	}

	/// Check whether the player body (a thin vertical column) overlaps any
	/// solid block at the given position.  We check at feet level and at
	/// mid-body level so the player cannot walk through one-block-high walls
	/// or two-block-high walls.
	bool isCollidingHorizontally(WorldGridComponent *grid, const Vector3 &pos) const
	{
		float bs = grid->GetBlockSize();
		float halfBody = bs * 0.3f; // player body half-width (~60% of a block)

		// Heights to test: feet + small offset, and mid-body
		float testHeights[] = {pos.y + 1.0f, pos.y + eyeHeight * 0.5f};

		// Test four horizontal offsets (approximate the body as a small square)
		float offsets[][2] = {
			{halfBody, 0.0f},
			{-halfBody, 0.0f},
			{0.0f, halfBody},
			{0.0f, -halfBody}};

		for (float h : testHeights)
		{
			for (auto &off : offsets)
			{
				int gx, gy, gz;
				Vector3 probe(pos.x + off[0], h, pos.z + off[1]);
				if (grid->WorldToGrid(probe, gx, gy, gz))
				{
					if (grid->GetBlock(gx, gy, gz))
					{
						return true;
					}
				}
			}
			// Also check the center
			int gx, gy, gz;
			if (grid->WorldToGrid(Vector3(pos.x, h, pos.z), gx, gy, gz))
			{
				if (grid->GetBlock(gx, gy, gz))
				{
					return true;
				}
			}
		}
		return false;
	}

	void pushOutOfBlocks(WorldGridComponent *grid, Vector3 &pos) const
	{
		float bs = grid->GetBlockSize();
		float checkY = pos.y + 1.0f;
		int gx, gy, gz;
		if (!grid->WorldToGrid(Vector3(pos.x, checkY, pos.z), gx, gy, gz))
			return;
		if (!grid->GetBlock(gx, gy, gz))
			return;

		Vector3 blockCenter = grid->GridToWorld(gx, gy, gz);
		float dx = pos.x - blockCenter.x;
		float dz = pos.z - blockCenter.z;

		// Determine which face is closest for pushout
		float absDx = std::abs(dx);
		float absDz = std::abs(dz);
		float halfBlock = bs * 0.5f;
		float halfBody = bs * 0.3f;

		if (absDx >= absDz)
		{
			// Push along X
			if (dx >= 0)
			{
				pos.x = blockCenter.x + halfBlock + halfBody + 0.1f;
			}
			else
			{
				pos.x = blockCenter.x - halfBlock - halfBody - 0.1f;
			}
		}
		else
		{
			// Push along Z
			if (dz >= 0)
			{
				pos.z = blockCenter.z + halfBlock + halfBody + 0.1f;
			}
			else
			{
				pos.z = blockCenter.z - halfBlock - halfBody - 0.1f;
			}
		}
	}

private:
	// Movement
	float moveSpeed;
	float velocityY;
	float gravity;
	float jumpSpeed;
	bool isGrounded;

	// Camera
	Object *cameraObject;
	float eyeHeight;
	float yaw;
	float pitch;
	float mouseSensitivity;

	WorldGridComponent *cachedGrid = nullptr;
	HotbarComponent *hotbar = nullptr;

	bool rayHitValid = false;
	int rayHitGx = 0, rayHitGy = 0, rayHitGz = 0; 
	bool rayHasEmpty = false;
	int rayEmptyGx = 0, rayEmptyGy = 0, rayEmptyGz = 0;

	float hoverRayTimer = 0.0f;
	static constexpr float kHoverRayInterval = 0.03f; 
};
