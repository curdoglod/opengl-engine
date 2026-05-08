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
		rayPlaceValid = false;

		if (grid && cameraObject)
		{
			Vector3 rayStart = cameraObject->GetPosition3D();
			Vector3 rayDir = getLookDirection();
			float stepSize = grid->GetBlockSize() * 0.4f;
			Vector3 currentPos = rayStart;
			bool hasPreviousEmpty = false;
			int previousGx = 0, previousGy = 0, previousGz = 0;

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
						if (hasPreviousEmpty && isNeighborCell(previousGx, previousGy, previousGz, gx, gy, gz))
						{
							rayPlaceValid = true;
							rayPlaceGx = previousGx; rayPlaceGy = previousGy; rayPlaceGz = previousGz;
						}
						else
						{
							setPlacementCellFromHit(grid, currentPos, gx, gy, gz);
						}
						break;
					}
					else
					{
						hasPreviousEmpty = true;
						previousGx = gx; previousGy = gy; previousGz = gz;
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

	bool isNeighborCell(int ax, int ay, int az, int bx, int by, int bz) const
	{
		int dist = std::abs(ax - bx) + std::abs(ay - by) + std::abs(az - bz);
		return dist == 1;
	}

	void setPlacementCellFromHit(WorldGridComponent *grid, const Vector3 &hitPos, int gx, int gy, int gz)
	{
		Vector3 center = grid->GridToWorld(gx, gy, gz);
		float dx = hitPos.x - center.x;
		float dy = hitPos.y - center.y;
		float dz = hitPos.z - center.z;

		float ax = std::abs(dx);
		float ay = std::abs(dy);
		float az = std::abs(dz);

		int px = gx;
		int py = gy;
		int pz = gz;

		if (ax >= ay && ax >= az)
			px += (dx >= 0.0f) ? 1 : -1;
		else if (ay >= ax && ay >= az)
			py += (dy >= 0.0f) ? 1 : -1;
		else
			pz += (dz >= 0.0f) ? 1 : -1;

		if (py >= 0 && !grid->HasBlock(px, py, pz))
		{
			rayPlaceValid = true;
			rayPlaceGx = px; rayPlaceGy = py; rayPlaceGz = pz;
		}
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
		return 0.0f;
	}

	// Thin body check against nearby blocks.
	bool isCollidingHorizontally(WorldGridComponent *grid, const Vector3 &pos) const
	{
		float bs = grid->GetBlockSize();
		float halfBody = bs * 0.3f;

		float testHeights[] = {pos.y + 1.0f, pos.y + eyeHeight * 0.5f};

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

		float absDx = std::abs(dx);
		float absDz = std::abs(dz);
		float halfBlock = bs * 0.5f;
		float halfBody = bs * 0.3f;

		if (absDx >= absDz)
		{
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
	float moveSpeed;
	float velocityY;
	float gravity;
	float jumpSpeed;
	bool isGrounded;

	Object *cameraObject;
	float eyeHeight;
	float yaw;
	float pitch;
	float mouseSensitivity;

	WorldGridComponent *cachedGrid = nullptr;
	HotbarComponent *hotbar = nullptr;

	bool rayHitValid = false;
	int rayHitGx = 0, rayHitGy = 0, rayHitGz = 0; 
	bool rayPlaceValid = false;
	int rayPlaceGx = 0, rayPlaceGy = 0, rayPlaceGz = 0;

	float hoverRayTimer = 0.0f;
	static constexpr float kHoverRayInterval = 0.03f; 
};
