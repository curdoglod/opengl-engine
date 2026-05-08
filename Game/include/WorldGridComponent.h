#pragma once

#include "component.h"
#include "object.h"
#include "Scene.h"
#include "BlockComponent.h"
#include "CameraComponent.h"
#include "ResourceManager.h"
#include "VoxelRenderer.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <climits>
#include <cmath>
#include <deque>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// Voxel world stored as chunks, not one Object per block.

static constexpr int CHUNK_SIZE = 16;

struct ChunkCoord {
	int cx = 0;
	int cz = 0;

	bool operator==(const ChunkCoord& other) const {
		return cx == other.cx && cz == other.cz;
	}
};

struct ChunkCoordHash {
	size_t operator()(const ChunkCoord& coord) const {
		long long packed = ((long long)coord.cx << 32) | (unsigned int)coord.cz;
		return std::hash<long long>()(packed);
	}
};

struct Chunk {
	ChunkCoord coord;
	std::unordered_map<int, BlockType> blocks;
	bool generated = false;
	bool meshDirty = true;

	// Local block coordinates packed into one map key.
	static int PackLocal(int lx, int ly, int lz) {
		return (ly << 8) | (lz << 4) | lx;
	}

	static void UnpackLocal(int key, int& lx, int& ly, int& lz) {
		lx = key & 0xF;
		lz = (key >> 4) & 0xF;
		ly = key >> 8;
	}
};

class WorldGridComponent : public Component {
public:
	WorldGridComponent()
		: blockSize(20.0f / 35.0f)
		, renderDistance(4)
		, worldSeed(std::random_device{}())
		, baseHeight(3)
		, maxHillHeight(6)
		, surfaceType(BlockType::Dirt)
		, undergroundType(BlockType::Stone)
		, lastPlayerCx(INT_MAX)
		, lastPlayerCz(INT_MAX)
	{}

	~WorldGridComponent() {
		VoxelRenderer::Get().Clear();
		for (auto& pair : chunks) {
			delete pair.second;
		}
		chunks.clear();
	}

	void Init() override {
		VoxelRenderer::Get().Init();
		preloadTextures();
	}

	void Update(float dt) override {
		(void)dt;

		updateChunksAroundPlayer();
		processGenerationQueue();
		rebuildDirtyMeshes();
	}

	// Setup

	void SetBlockSize(float size) { blockSize = size; }
	float GetBlockSize() const { return blockSize; }

	void SetRenderDistance(int chunksFromPlayer) { renderDistance = chunksFromPlayer; }
	void SetSeed(unsigned int seed) { worldSeed = seed; }

	void SetTerrainParams(int base, int hill, BlockType surface, BlockType underground) {
		baseHeight = base;
		maxHillHeight = hill;
		surfaceType = surface;
		undergroundType = underground;
	}

	// Old fixed-grid API. Chunks do not need these values.
	void SetSize(int, int) {}
	void SetOrigin(float, float) {}
	void SetMaxRenderDistance(float) {}

	void GenerateFlat(BlockType type) {
		surfaceType = type;
		undergroundType = type;
		baseHeight = 1;
		maxHillHeight = 0;
	}

	void GenerateHillyTerrain(int base,
							  int hill,
							  BlockType surface,
							  BlockType underground,
							  unsigned int seed = std::random_device{}()) {
		worldSeed = seed;
		baseHeight = base;
		maxHillHeight = hill;
		surfaceType = surface;
		undergroundType = underground;
	}

	void SetCameraObject(Object* camera) {
		cameraObj = camera;
	}

	// Coordinates

	bool WorldToGrid(const Vector3& world, int& gx, int& gy, int& gz) const {
		gx = (int)std::floor(world.x / blockSize + 0.5f);
		gy = (int)std::floor(world.y / blockSize + 0.5f);
		gz = (int)std::floor(world.z / blockSize + 0.5f);
		return gy >= 0;
	}

	Vector3 GridToWorld(int gx, int gy, int gz) const {
		return Vector3(gx * blockSize, gy * blockSize, gz * blockSize);
	}

	float GetSpawnHeight(int gx, int gz) const {
		return (getTerrainHeight(gx, gz) + 1) * blockSize;
	}

	// Blocks

	bool HasBlock(int gx, int gy, int gz) const {
		const Chunk* chunk = findGeneratedChunkForBlock(gx, gz);
		if (!chunk) return false;

		int lx, lz;
		worldToLocalBlock(gx, gz, lx, lz);
		return chunk->blocks.count(Chunk::PackLocal(lx, gy, lz)) > 0;
	}

	Object* GetBlock(int gx, int gy, int gz) const {
		// Blocks are data now; old callers only need a non-null value.
		return HasBlock(gx, gy, gz) ? reinterpret_cast<Object*>(1) : nullptr;
	}

	BlockType GetBlockType(int gx, int gy, int gz) const {
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);

		auto chunkIt = chunks.find({cx, cz});
		if (chunkIt == chunks.end()) return BlockType::Dirt;

		auto blockIt = chunkIt->second->blocks.find(Chunk::PackLocal(lx, gy, lz));
		return (blockIt != chunkIt->second->blocks.end()) ? blockIt->second : BlockType::Dirt;
	}

	Object* CreateBlockAt(int gx, int gy, int gz, BlockType type) {
		if (gy < 0 || HasBlock(gx, gy, gz)) return nullptr;

		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);

		Chunk* chunk = getOrCreateChunk(cx, cz);
		chunk->blocks[Chunk::PackLocal(lx, gy, lz)] = type;
		markChunkAndBorderDirty(chunk, gx, gz);

		return reinterpret_cast<Object*>(1);
	}

	void RemoveBlockAt(int gx, int gy, int gz) {
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);

		auto chunkIt = chunks.find({cx, cz});
		if (chunkIt == chunks.end()) return;

		int key = Chunk::PackLocal(lx, gy, lz);
		if (chunkIt->second->blocks.erase(key) > 0) {
			markChunkAndBorderDirty(chunkIt->second, gx, gz);
		}
	}

	Object* GetBlock(int gx, int gz) const { return GetBlock(gx, 0, gz); }
	Object* CreateBlockAt(int gx, int gz, BlockType type) { return CreateBlockAt(gx, 0, gz, type); }
	void RemoveBlockAt(int gx, int gz) { RemoveBlockAt(gx, 0, gz); }

	// Build the spawn area immediately.
	void ForceGenerateArea(int gx, int gz, int radiusChunks = 1) {
		int centerCx, centerCz, lx, lz;
		globalToChunk(gx, gz, centerCx, centerCz, lx, lz);

		for (int dz = -radiusChunks; dz <= radiusChunks; ++dz) {
			for (int dx = -radiusChunks; dx <= radiusChunks; ++dx) {
				generateChunk(centerCx + dx, centerCz + dz);
			}
		}

		for (int dz = -radiusChunks; dz <= radiusChunks; ++dz) {
			for (int dx = -radiusChunks; dx <= radiusChunks; ++dx) {
				Chunk* chunk = findChunk(centerCx + dx, centerCz + dz);
				if (chunk && chunk->meshDirty) {
					buildChunkMesh(chunk);
				}
			}
		}
	}

	void SetHighlightBlock(int gx, int gy, int gz) {
		glm::vec3 worldPos(gx * blockSize, gy * blockSize, gz * blockSize);
		VoxelRenderer::Get().SetHighlight(worldPos, true, blockSize * 0.5f);
	}

	void ClearHighlight() {
		VoxelRenderer::Get().SetHighlight(glm::vec3(0.0f), false, blockSize * 0.5f);
	}

private:
	static constexpr int CHUNKS_GENERATED_PER_FRAME = 8;
	static constexpr int CHUNK_MESHES_REBUILT_PER_FRAME = 8;
	static constexpr int CHUNK_UNLOAD_MARGIN = 2;
	static constexpr int FLOATS_PER_VERTEX = 8;

	enum FaceDirection {
		FacePositiveX = 0,
		FaceNegativeX = 1,
		FacePositiveY = 2,
		FaceNegativeY = 3,
		FacePositiveZ = 4,
		FaceNegativeZ = 5
	};

	// Chunk coordinates

	static void globalToChunk(int gx, int gz, int& cx, int& cz, int& lx, int& lz) {
		cx = floorDiv(gx, CHUNK_SIZE);
		cz = floorDiv(gz, CHUNK_SIZE);
		lx = gx - cx * CHUNK_SIZE;
		lz = gz - cz * CHUNK_SIZE;
	}

	static int floorDiv(int value, int divisor) {
		return (value >= 0) ? (value / divisor) : ((value - divisor + 1) / divisor);
	}

	static void worldToLocalBlock(int gx, int gz, int& lx, int& lz) {
		int cx, cz;
		globalToChunk(gx, gz, cx, cz, lx, lz);
	}

	Chunk* findChunk(int cx, int cz) {
		auto it = chunks.find({cx, cz});
		return (it != chunks.end()) ? it->second : nullptr;
	}

	const Chunk* findGeneratedChunkForBlock(int gx, int gz) const {
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);

		auto it = chunks.find({cx, cz});
		if (it == chunks.end() || !it->second->generated) return nullptr;
		return it->second;
	}

	Chunk* getOrCreateChunk(int cx, int cz) {
		ChunkCoord coord{cx, cz};
		auto it = chunks.find(coord);
		if (it != chunks.end()) return it->second;

		Chunk* chunk = new Chunk();
		chunk->coord = coord;
		chunks[coord] = chunk;
		return chunk;
	}

	// Terrain height

	static float hashNoise(int x, int z, unsigned int seed) {
		unsigned int n = (unsigned int)(x * 73856093) ^ (unsigned int)(z * 19349663) ^ seed;
		n = (n << 13) ^ n;
		n = n * (n * n * 15731u + 789221u) + 1376312589u;
		return (float)(n & 0x7FFFFFFF) / (float)0x7FFFFFFF;
	}

	static float sampleNoise(int gx, int gz, int gridStep, unsigned int seed) {
		float fx = (float)gx / (float)gridStep;
		float fz = (float)gz / (float)gridStep;

		int ix = (int)std::floor(fx);
		int iz = (int)std::floor(fz);

		float tx = smoothStep(fx - (float)ix);
		float tz = smoothStep(fz - (float)iz);

		float v00 = hashNoise(ix,     iz,     seed);
		float v10 = hashNoise(ix + 1, iz,     seed);
		float v01 = hashNoise(ix,     iz + 1, seed);
		float v11 = hashNoise(ix + 1, iz + 1, seed);

		float top = lerp(v00, v10, tx);
		float bottom = lerp(v01, v11, tx);
		return lerp(top, bottom, tz);
	}

	static float smoothStep(float t) {
		return t * t * (3.0f - 2.0f * t);
	}

	static float lerp(float a, float b, float t) {
		return a + (b - a) * t;
	}

	int getTerrainHeight(int gx, int gz) const {
		float largeHills = sampleNoise(gx, gz, 8, worldSeed);
		float mediumHills = sampleNoise(gx, gz, 4, worldSeed * 2u + 137u);
		float smallVariation = sampleNoise(gx, gz, 2, worldSeed * 3u + 5449u);

		float combined = largeHills * 0.6f + mediumHills * 0.25f + smallVariation * 0.15f;
		return std::max(1, baseHeight + (int)(combined * maxHillHeight));
	}

	// Chunk streaming

	void updateChunksAroundPlayer() {
		if (!object || !object->GetScene()) return;

		Vector3 cameraPosition = getCameraPosition();

		int gx, gy, gz;
		WorldToGrid(cameraPosition, gx, gy, gz);

		int playerCx, playerCz, lx, lz;
		globalToChunk(gx, gz, playerCx, playerCz, lx, lz);

		if (playerCx == lastPlayerCx && playerCz == lastPlayerCz) return;
		lastPlayerCx = playerCx;
		lastPlayerCz = playerCz;

		queueChunksAround(playerCx, playerCz);
		unloadDistantChunks(playerCx, playerCz);
	}

	Vector3 getCameraPosition() {
		if (cameraObj) {
			return cameraObj->GetPosition3D();
		}

		const auto& objects = object->GetScene()->GetObjects();
		for (auto* sceneObject : objects) {
			if (!sceneObject) continue;

			auto* camera = sceneObject->GetComponent<CameraComponent>();
			if (camera) {
				cameraObj = sceneObject;
				return sceneObject->GetPosition3D();
			}
		}

		return Vector3(0, 0, 0);
	}

	void queueChunksAround(int centerCx, int centerCz) {
		for (int dz = -renderDistance; dz <= renderDistance; ++dz) {
			for (int dx = -renderDistance; dx <= renderDistance; ++dx) {
				int cx = centerCx + dx;
				int cz = centerCz + dz;

				Chunk* chunk = findChunk(cx, cz);
				if (!chunk || !chunk->generated) {
					enqueueChunk(cx, cz);
				}
			}
		}
	}

	void unloadDistantChunks(int centerCx, int centerCz) {
		int unloadDistance = renderDistance + CHUNK_UNLOAD_MARGIN;
		std::vector<ChunkCoord> chunksToUnload;

		for (const auto& pair : chunks) {
			int dx = pair.first.cx - centerCx;
			int dz = pair.first.cz - centerCz;

			if (std::abs(dx) > unloadDistance || std::abs(dz) > unloadDistance) {
				chunksToUnload.push_back(pair.first);
			}
		}

		for (const auto& coord : chunksToUnload) {
			unloadChunk(coord.cx, coord.cz);
		}
	}

	void enqueueChunk(int cx, int cz) {
		ChunkCoord coord{cx, cz};

		Chunk* chunk = findChunk(cx, cz);
		if (chunk && chunk->generated) return;

		for (const auto& queuedCoord : generateQueue) {
			if (queuedCoord == coord) return;
		}

		generateQueue.push_back(coord);
	}

	void processGenerationQueue() {
		if (!object || !object->GetScene()) return;

		for (int i = 0; i < CHUNKS_GENERATED_PER_FRAME && !generateQueue.empty(); ++i) {
			ChunkCoord coord = generateQueue.front();
			generateQueue.pop_front();

			Chunk* chunk = findChunk(coord.cx, coord.cz);
			if (chunk && chunk->generated) continue;

			generateChunk(coord.cx, coord.cz);
		}
	}

	void generateChunk(int cx, int cz) {
		Chunk* chunk = getOrCreateChunk(cx, cz);
		if (chunk->generated) return;

		int startGx = cx * CHUNK_SIZE;
		int startGz = cz * CHUNK_SIZE;

		for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
			for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
				int gx = startGx + lx;
				int gz = startGz + lz;
				int height = getTerrainHeight(gx, gz);

				for (int gy = 0; gy < height; ++gy) {
					BlockType type = (gy == height - 1) ? surfaceType : undergroundType;
					chunk->blocks[Chunk::PackLocal(lx, gy, lz)] = type;
				}
			}
		}

		chunk->generated = true;
		chunk->meshDirty = true;
		markGeneratedNeighborsDirty(cx, cz);
	}

	void unloadChunk(int cx, int cz) {
		ChunkCoord coord{cx, cz};

		generateQueue.erase(
			std::remove_if(generateQueue.begin(), generateQueue.end(),
				[&](const ChunkCoord& queuedCoord) { return queuedCoord == coord; }),
			generateQueue.end());

		auto chunkIt = chunks.find(coord);
		if (chunkIt == chunks.end()) return;

		VoxelRenderer::Get().RemoveChunk(cx, cz);
		delete chunkIt->second;
		chunks.erase(chunkIt);
	}

	void markGeneratedNeighborsDirty(int cx, int cz) {
		static const int neighborDx[] = {-1, 1, 0, 0};
		static const int neighborDz[] = {0, 0, -1, 1};

		for (int i = 0; i < 4; ++i) {
			Chunk* neighbor = findChunk(cx + neighborDx[i], cz + neighborDz[i]);
			if (neighbor && neighbor->generated) {
				neighbor->meshDirty = true;
			}
		}
	}

	// Meshes

	void rebuildDirtyMeshes() {
		int rebuilt = 0;

		for (auto& pair : chunks) {
			Chunk* chunk = pair.second;
			if (!chunk->generated || !chunk->meshDirty) continue;

			buildChunkMesh(chunk);
			if (++rebuilt >= CHUNK_MESHES_REBUILT_PER_FRAME) break;
		}
	}

	void buildChunkMesh(Chunk* chunk) {
		if (chunk->blocks.empty()) {
			chunk->meshDirty = false;
			VoxelRenderer::Get().RemoveChunk(chunk->coord.cx, chunk->coord.cz);
			return;
		}

		std::unordered_map<int, std::vector<float>> verticesByType;
		std::unordered_map<int, std::vector<unsigned int>> indicesByType;

		for (const auto& block : chunk->blocks) {
			appendVisibleFacesForBlock(chunk, block.first, block.second, verticesByType, indicesByType);
		}

		std::vector<VoxelMeshData> meshes = buildRendererMeshes(verticesByType, indicesByType);
		VoxelRenderer::Get().UpdateChunk(chunk->coord.cx, chunk->coord.cz, meshes);

		chunk->meshDirty = false;
	}

	void appendVisibleFacesForBlock(Chunk* chunk,
									int packedBlockKey,
									BlockType type,
									std::unordered_map<int, std::vector<float>>& verticesByType,
									std::unordered_map<int, std::vector<unsigned int>>& indicesByType) {
		int lx, ly, lz;
		Chunk::UnpackLocal(packedBlockKey, lx, ly, lz);

		int gx = chunk->coord.cx * CHUNK_SIZE + lx;
		int gz = chunk->coord.cz * CHUNK_SIZE + lz;

		float wx = gx * blockSize;
		float wy = ly * blockSize;
		float wz = gz * blockSize;
		float halfSize = blockSize * 0.5f;
		int typeKey = (int)type;

		auto& vertices = verticesByType[typeKey];
		auto& indices = indicesByType[typeKey];

		if (!hasBlockAt(gx + 1, ly, gz)) appendFace(vertices, indices, wx, wy, wz, halfSize, FacePositiveX);
		if (!hasBlockAt(gx - 1, ly, gz)) appendFace(vertices, indices, wx, wy, wz, halfSize, FaceNegativeX);
		if (!hasBlockAt(gx, ly + 1, gz)) appendFace(vertices, indices, wx, wy, wz, halfSize, FacePositiveY);
		if (ly == 0 || !hasBlockAt(gx, ly - 1, gz)) appendFace(vertices, indices, wx, wy, wz, halfSize, FaceNegativeY);
		if (!hasBlockAt(gx, ly, gz + 1)) appendFace(vertices, indices, wx, wy, wz, halfSize, FacePositiveZ);
		if (!hasBlockAt(gx, ly, gz - 1)) appendFace(vertices, indices, wx, wy, wz, halfSize, FaceNegativeZ);
	}

	std::vector<VoxelMeshData> buildRendererMeshes(
		std::unordered_map<int, std::vector<float>>& verticesByType,
		std::unordered_map<int, std::vector<unsigned int>>& indicesByType) const {
		std::vector<VoxelMeshData> meshes;

		for (auto& pair : verticesByType) {
			int typeKey = pair.first;
			auto& vertices = pair.second;
			auto& indices = indicesByType[typeKey];
			if (indices.empty()) continue;

			VoxelMeshData mesh;
			mesh.textureId = getTextureForType((BlockType)typeKey);
			mesh.vertices = std::move(vertices);
			mesh.indices = std::move(indices);
			meshes.push_back(std::move(mesh));
		}

		return meshes;
	}

	bool hasBlockAt(int gx, int gy, int gz) const {
		if (gy < 0) return true;

		const Chunk* chunk = findGeneratedChunkForBlock(gx, gz);
		if (!chunk) return false;

		int lx, lz;
		worldToLocalBlock(gx, gz, lx, lz);
		return chunk->blocks.count(Chunk::PackLocal(lx, gy, lz)) > 0;
	}

	void markChunkAndBorderDirty(Chunk* chunk, int gx, int gz) {
		chunk->meshDirty = true;
		markBorderNeighborsDirty(gx, gz);
	}

	void markBorderNeighborsDirty(int gx, int gz) {
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);

		if (lx == 0) markChunkDirty(cx - 1, cz);
		if (lx == CHUNK_SIZE - 1) markChunkDirty(cx + 1, cz);
		if (lz == 0) markChunkDirty(cx, cz - 1);
		if (lz == CHUNK_SIZE - 1) markChunkDirty(cx, cz + 1);
	}

	void markChunkDirty(int cx, int cz) {
		Chunk* chunk = findChunk(cx, cz);
		if (chunk) {
			chunk->meshDirty = true;
		}
	}

	// Vertex layout: position.xyz, normal.xyz, uv.xy.
	static void appendFace(std::vector<float>& vertices,
						   std::vector<unsigned int>& indices,
						   float centerX,
						   float centerY,
						   float centerZ,
						   float halfSize,
						   FaceDirection face) {
		unsigned int base = (unsigned int)(vertices.size() / FLOATS_PER_VERTEX);
		float v[4][FLOATS_PER_VERTEX];

		fillFaceVertices(v, centerX, centerY, centerZ, halfSize, face);

		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < FLOATS_PER_VERTEX; ++j) {
				vertices.push_back(v[i][j]);
			}
		}

		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
		indices.push_back(base);
		indices.push_back(base + 2);
		indices.push_back(base + 3);
	}

	static void fillFaceVertices(float v[4][FLOATS_PER_VERTEX],
								 float centerX,
								 float centerY,
								 float centerZ,
								 float h,
								 FaceDirection face) {
		switch (face) {
			case FacePositiveX:
				setVertex(v[0], centerX + h, centerY - h, centerZ - h,  1,  0,  0, 0, 0);
				setVertex(v[1], centerX + h, centerY + h, centerZ - h,  1,  0,  0, 0, 1);
				setVertex(v[2], centerX + h, centerY + h, centerZ + h,  1,  0,  0, 1, 1);
				setVertex(v[3], centerX + h, centerY - h, centerZ + h,  1,  0,  0, 1, 0);
				break;

			case FaceNegativeX:
				setVertex(v[0], centerX - h, centerY - h, centerZ + h, -1,  0,  0, 0, 0);
				setVertex(v[1], centerX - h, centerY + h, centerZ + h, -1,  0,  0, 0, 1);
				setVertex(v[2], centerX - h, centerY + h, centerZ - h, -1,  0,  0, 1, 1);
				setVertex(v[3], centerX - h, centerY - h, centerZ - h, -1,  0,  0, 1, 0);
				break;

			case FacePositiveY:
				setVertex(v[0], centerX - h, centerY + h, centerZ - h,  0,  1,  0, 0, 0);
				setVertex(v[1], centerX - h, centerY + h, centerZ + h,  0,  1,  0, 0, 1);
				setVertex(v[2], centerX + h, centerY + h, centerZ + h,  0,  1,  0, 1, 1);
				setVertex(v[3], centerX + h, centerY + h, centerZ - h,  0,  1,  0, 1, 0);
				break;

			case FaceNegativeY:
				setVertex(v[0], centerX - h, centerY - h, centerZ + h,  0, -1,  0, 0, 0);
				setVertex(v[1], centerX - h, centerY - h, centerZ - h,  0, -1,  0, 0, 1);
				setVertex(v[2], centerX + h, centerY - h, centerZ - h,  0, -1,  0, 1, 1);
				setVertex(v[3], centerX + h, centerY - h, centerZ + h,  0, -1,  0, 1, 0);
				break;

			case FacePositiveZ:
				setVertex(v[0], centerX - h, centerY - h, centerZ + h,  0,  0,  1, 0, 0);
				setVertex(v[1], centerX + h, centerY - h, centerZ + h,  0,  0,  1, 1, 0);
				setVertex(v[2], centerX + h, centerY + h, centerZ + h,  0,  0,  1, 1, 1);
				setVertex(v[3], centerX - h, centerY + h, centerZ + h,  0,  0,  1, 0, 1);
				break;

			case FaceNegativeZ:
				setVertex(v[0], centerX + h, centerY - h, centerZ - h,  0,  0, -1, 0, 0);
				setVertex(v[1], centerX - h, centerY - h, centerZ - h,  0,  0, -1, 1, 0);
				setVertex(v[2], centerX - h, centerY + h, centerZ - h,  0,  0, -1, 1, 1);
				setVertex(v[3], centerX + h, centerY + h, centerZ - h,  0,  0, -1, 0, 1);
				break;
		}
	}

	static void setVertex(float out[FLOATS_PER_VERTEX],
						  float x,
						  float y,
						  float z,
						  float nx,
						  float ny,
						  float nz,
						  float u,
						  float v) {
		out[0] = x;
		out[1] = y;
		out[2] = z;
		out[3] = nx;
		out[4] = ny;
		out[5] = nz;
		out[6] = u;
		out[7] = v;
	}

	// Textures

	void preloadTextures() {
		auto& resources = ResourceManager::Get();
		texDirt = resources.LoadTexture("Assets/block_textures/dirt.png");
		texStone = resources.LoadTexture("Assets/block_textures/stone.png");
		texGrass = resources.LoadTexture("Assets/block_textures/grass.png");
		texSand = resources.LoadTexture("Assets/block_textures/sand.png");
		texWood = resources.LoadTexture("Assets/block_textures/wood.png");
	}

	GLuint getTextureForType(BlockType type) const {
		switch (type) {
			case BlockType::Dirt: return texDirt;
			case BlockType::Stone: return texStone;
			case BlockType::Grass: return texGrass;
			case BlockType::Sand: return texSand;
			case BlockType::Wood: return texWood;
		}

		return texDirt;
	}

private:
	float blockSize;
	int renderDistance;
	unsigned int worldSeed;

	int baseHeight;
	int maxHillHeight;
	BlockType surfaceType;
	BlockType undergroundType;

	std::unordered_map<ChunkCoord, Chunk*, ChunkCoordHash> chunks;
	std::deque<ChunkCoord> generateQueue;

	Object* cameraObj = nullptr;
	int lastPlayerCx;
	int lastPlayerCz;

	GLuint texDirt = 0;
	GLuint texStone = 0;
	GLuint texGrass = 0;
	GLuint texSand = 0;
	GLuint texWood = 0;
};
