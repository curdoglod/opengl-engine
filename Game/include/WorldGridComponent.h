#pragma once

#include "component.h"
#include "object.h"
#include "Scene.h"
#include "BlockComponent.h"
#include "Model3DComponent.h"

#include <cmath>
#include <unordered_map>


class WorldGridComponent : public Component {
public:
	static constexpr int WORLD_SIZE_XZ = 25;

	WorldGridComponent()
		: blockSize(20.0f / 35.0f),
		  surfaceType(BlockType::Grass),
		  undergroundType(BlockType::Stone),
		  highlightedBlock(nullptr) {}

	~WorldGridComponent() override {
		// Scene owns the Object*; it cleans them up when the scene dies.
		blocks.clear();
	}

	void Init() override {}
	void Update(float dt) override { (void)dt; }

	// Setup -------------------------------------------------------------------

	void SetBlockSize(float size) { blockSize = size; }
	float GetBlockSize() const { return blockSize; }

	void SetCameraObject(Object* /*camera*/) {}

	void SetTerrainParams(int /*base*/, int /*hill*/, BlockType surface, BlockType underground) {
		surfaceType = surface;
		undergroundType = underground;
	}

	// Stubs kept so existing scene code still compiles.
	void SetRenderDistance(int) {}
	void SetSeed(unsigned int) {}
	void SetSize(int, int) {}
	void SetOrigin(float, float) {}
	void SetMaxRenderDistance(float) {}

	void GenerateFlat(BlockType type) {
		surfaceType = type;
		undergroundType = type;
	}

	void GenerateHillyTerrain(int, int, BlockType surface, BlockType underground, unsigned int = 0) {
		surfaceType = surface;
		undergroundType = underground;
	}

	// Coordinates -------------------------------------------------------------

	bool WorldToGrid(const Vector3& world, int& gx, int& gy, int& gz) const {
		gx = (int)std::floor(world.x / blockSize + 0.5f);
		gy = (int)std::floor(world.y / blockSize + 0.5f);
		gz = (int)std::floor(world.z / blockSize + 0.5f);
		return gy >= 0;
	}

	Vector3 GridToWorld(int gx, int gy, int gz) const {
		return Vector3(gx * blockSize, gy * blockSize, gz * blockSize);
	}

	float GetSpawnHeight(int /*gx*/, int /*gz*/) const {
		// Top layer is at gy = TOP_LAYER. Player stands on top of it.
		return (TOP_LAYER + 1) * blockSize;
	}

	// Blocks ------------------------------------------------------------------

	bool HasBlock(int gx, int gy, int gz) const {
		return blocks.find(packKey(gx, gy, gz)) != blocks.end();
	}

	Object* GetBlock(int gx, int gy, int gz) const {
		auto it = blocks.find(packKey(gx, gy, gz));
		return (it != blocks.end()) ? it->second : nullptr;
	}

	BlockType GetBlockType(int gx, int gy, int gz) const {
		Object* obj = GetBlock(gx, gy, gz);
		if (!obj) return BlockType::Dirt;
		BlockComponent* bc = obj->GetComponent<BlockComponent>();
		return bc ? bc->GetType() : BlockType::Dirt;
	}

	Object* CreateBlockAt(int gx, int gy, int gz, BlockType type) {
		if (gy < 0) return nullptr;
		if (!object || !object->GetScene()) return nullptr;
		if (HasBlock(gx, gy, gz)) return nullptr;

		Object* obj = object->GetScene()->CreateObject();
		obj->SetPosition(GridToWorld(gx, gy, gz));
		obj->SetSize(Vector3(blockSize, blockSize, blockSize));

		BlockComponent* bc = new BlockComponent();
		obj->AddComponent(bc);
		bc->SetType(type);

		if (auto* model = obj->GetComponent<Model3DComponent>()) {
			model->SetSizeIsRelative(false);
		}

		blocks[packKey(gx, gy, gz)] = obj;
		return obj;
	}

	void RemoveBlockAt(int gx, int gy, int gz) {
		auto it = blocks.find(packKey(gx, gy, gz));
		if (it == blocks.end()) return;

		Object* obj = it->second;
		if (obj == highlightedBlock) {
			highlightedBlock = nullptr;
		}

		if (obj && object && object->GetScene()) {
			object->GetScene()->DeleteObject(obj);
		}
		blocks.erase(it);
	}

	Object* GetBlock(int gx, int gz) const { return GetBlock(gx, 0, gz); }
	Object* CreateBlockAt(int gx, int gz, BlockType type) { return CreateBlockAt(gx, 0, gz, type); }
	void RemoveBlockAt(int gx, int gz) { RemoveBlockAt(gx, 0, gz); }


	void ForceGenerateArea(int /*gx*/, int /*gz*/, int /*radiusChunks*/ = 1) {
		if (generated) return;

		const int half = WORLD_SIZE_XZ / 2;
		for (int gx = -half; gx < half; ++gx) {
			for (int gz = -half; gz < half; ++gz) {
				CreateBlockAt(gx, 0, gz, BlockType::Stone);
				CreateBlockAt(gx, 1, gz, BlockType::Stone);
				CreateBlockAt(gx, 2, gz, undergroundType);
				CreateBlockAt(gx, TOP_LAYER, gz, surfaceType);
			}
		}
		generated = true;
	}

	// Highlight ---------------------------------------------------------------

	void SetHighlightBlock(int gx, int gy, int gz) {
		Object* target = GetBlock(gx, gy, gz);
		if (target == highlightedBlock) return;

		clearHighlightOn(highlightedBlock);
		highlightedBlock = target;
		applyHighlightOn(highlightedBlock, true);
	}

	void ClearHighlight() {
		clearHighlightOn(highlightedBlock);
		highlightedBlock = nullptr;
	}

private:
	static constexpr int TOP_LAYER = 3;

	static long long packKey(int gx, int gy, int gz) {
		const long long bias = 1LL << 20;
		return ((long long)(gx + bias) << 42)
			 | ((long long)(gy + bias) << 21)
			 |  (long long)(gz + bias);
	}

	void applyHighlightOn(Object* obj, bool enabled) {
		if (!obj) return;
		Model3DComponent* model = obj->GetComponent<Model3DComponent>();
		if (!model) return;

		model->SetHighlight(enabled, glm::vec3(1.0f, 0.95f, 0.35f), 0.35f);
	}

	void clearHighlightOn(Object* obj) {
		applyHighlightOn(obj, false);
	}

private:
	float blockSize;
	BlockType surfaceType;
	BlockType undergroundType;

	std::unordered_map<long long, Object*> blocks;
	Object* highlightedBlock;
	bool generated = false;
};
