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
	static constexpr int kRaycastSteps = 48;
	static constexpr float kRaycastStepFraction = 0.2f; // step = blockSize * fraction
	static constexpr float kFallRespawnWorldY = -3.0f;

public:
	PlayerController()
		: moveSpeed(120.0f / 35.0f), cameraObject(nullptr), eyeHeight(2.0f), yaw(0.0f), pitch(0.0f), mouseSensitivity(0.15f), velocityY(0.0f), gravity(-600.0f / 35.0f), isGrounded(false), jumpSpeed(220.0f / 35.0f)
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
			float stepSize = grid->GetBlockSize() * kRaycastStepFraction;
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

	// Block AABB at (px,py,pz) overlapping the player capsule means placing it
	// would trap the player inside geometry.
	bool placementOverlapsPlayer(WorldGridComponent *grid, int px, int py, int pz) const
	{
		if (!object) return false;
		float bs = grid->GetBlockSize();
		float half = bs * 0.5f;
		Vector3 center = grid->GridToWorld(px, py, pz);

		Vector3 pos = object->GetPosition3D();
		float bodyHalf = getBodyHalfWidth(grid);
		float playerMinY = pos.y;
		float playerMaxY = pos.y + eyeHeight;

		float blockMinX = center.x - half;
		float blockMaxX = center.x + half;
		float blockMinY = center.y - half;
		float blockMaxY = center.y + half;
		float blockMinZ = center.z - half;
		float blockMaxZ = center.z + half;

		return (pos.x - bodyHalf < blockMaxX) && (pos.x + bodyHalf > blockMinX)
			&& (playerMinY < blockMaxY) && (playerMaxY > blockMinY)
			&& (pos.z - bodyHalf < blockMaxZ) && (pos.z + bodyHalf > blockMinZ);
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


	float getBodyHalfWidth(WorldGridComponent *grid) const
	{
		return grid->GetBlockSize() * 0.3f;
	}

	int worldToGridIndex(float value, float blockSize) const
	{
		return (int)std::floor(value / blockSize + 0.5f);
	}

	bool isBodyColliding(WorldGridComponent *grid, const Vector3 &pos) const
	{
		float bs = grid->GetBlockSize();
		float halfBody = getBodyHalfWidth(grid);
		float inset = bs * 0.02f;

		int minGx = worldToGridIndex(pos.x - halfBody + inset, bs);
		int maxGx = worldToGridIndex(pos.x + halfBody - inset, bs);
		int minGy = worldToGridIndex(pos.y + inset, bs);
		int maxGy = worldToGridIndex(pos.y + eyeHeight - inset, bs);
		int minGz = worldToGridIndex(pos.z - halfBody + inset, bs);
		int maxGz = worldToGridIndex(pos.z + halfBody - inset, bs);

		for (int gy = minGy; gy <= maxGy; ++gy)
			for (int gz = minGz; gz <= maxGz; ++gz)
				for (int gx = minGx; gx <= maxGx; ++gx)
					if (grid->HasBlock(gx, gy, gz))
						return true;

		return false;
	}

	float highestBlockTopInsideBody(WorldGridComponent *grid, const Vector3 &pos) const
	{
		float bs = grid->GetBlockSize();
		float halfBody = getBodyHalfWidth(grid);
		float inset = bs * 0.02f;
		float result = -100000.0f;

		int minGx = worldToGridIndex(pos.x - halfBody + inset, bs);
		int maxGx = worldToGridIndex(pos.x + halfBody - inset, bs);
		int minGy = worldToGridIndex(pos.y, bs);
		int maxGy = worldToGridIndex(pos.y + eyeHeight - inset, bs);
		int minGz = worldToGridIndex(pos.z - halfBody + inset, bs);
		int maxGz = worldToGridIndex(pos.z + halfBody - inset, bs);

		for (int gy = minGy; gy <= maxGy; ++gy)
		{
			for (int gz = minGz; gz <= maxGz; ++gz)
			{
				for (int gx = minGx; gx <= maxGx; ++gx)
				{
					if (grid->HasBlock(gx, gy, gz))
					{
						Vector3 blockCenter = grid->GridToWorld(gx, gy, gz);
						result = std::max(result, blockCenter.y + bs * 0.5f);
					}
				}
			}
		}

		return result;
	}

	void moveHorizontal(WorldGridComponent *grid, Vector3 &pos, float dx, float dz) const
	{
		if (!grid)
		{
			pos.x += dx;
			pos.z += dz;
			return;
		}

		Vector3 testX(pos.x + dx, pos.y, pos.z);
		if (!isBodyColliding(grid, testX))
			pos.x += dx;

		Vector3 testZ(pos.x, pos.y, pos.z + dz);
		if (!isBodyColliding(grid, testZ))
			pos.z += dz;
	}

	void moveVertical(WorldGridComponent *grid, Vector3 &pos, float dt)
	{
		if (!grid)
		{
			velocityY += gravity * dt;
			pos.y += velocityY * dt;
			isGrounded = false;
			return;
		}


		if (isGrounded && velocityY <= 0.0f)
		{
			Vector3 probe(pos.x, pos.y - grid->GetBlockSize() * 0.05f, pos.z);
			if (isBodyColliding(grid, probe))
			{
				velocityY = 0.0f;
				return;
			}
			isGrounded = false;
		}

		velocityY += gravity * dt;
		isGrounded = false;

		float dy = velocityY * dt;
		Vector3 testY(pos.x, pos.y + dy, pos.z);

		if (!isBodyColliding(grid, testY))
		{
			pos.y += dy;
			return;
		}

		if (dy < 0.0f)
		{
			float groundY = highestBlockTopInsideBody(grid, testY);
			if (groundY > -99999.0f)
			{
				pos.y = groundY;
				isGrounded = true;
			}
		}

		velocityY = 0.0f;
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
