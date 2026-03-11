#pragma once
#include "component.h"
#include "object.h"
#include "Scene.h"
#include "BlockComponent.h"
#include "CameraComponent.h"
#include "LightComponent.h"
#include "ResourceManager.h"
#include "VoxelRenderer.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <unordered_map>
#include <string>
#include <random>
#include <cmath>
#include <vector>
#include <functional>
#include <climits>
#include <deque>
#include <iostream>

//  Instead of creating one Object per block, we store block data in arrays
//  and build a single mesh per chunk containing ONLY the exposed faces.
//  This reduces draw calls from ~5000+ to ~100 (2-3 per chunk).

static constexpr int CHUNK_SIZE = 16;

struct ChunkCoord {
	int cx, cz;
	bool operator==(const ChunkCoord& o) const { return cx == o.cx && cz == o.cz; }
};
struct ChunkCoordHash {
	size_t operator()(const ChunkCoord& c) const {
		return std::hash<long long>()(((long long)c.cx << 32) | (unsigned int)c.cz);
	}
};

struct Chunk {
	ChunkCoord coord;
	std::unordered_map<int, BlockType> blocks;
	bool generated = false;
	bool meshDirty = true;

	static int packLocal(int lx, int ly, int lz) {
		return (ly << 8) | (lz << 4) | lx;
	}
	static void unpackLocal(int key, int& lx, int& ly, int& lz) {
		lx = key & 0xF;
		lz = (key >> 4) & 0xF;
		ly = (key >> 8);
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
		for (auto& kv : chunks) delete kv.second;
		chunks.clear();
	}

	void Init() override {
		VoxelRenderer::Get().Init();
		preloadTextures();
	}

	void Update(float dt) override {
		updateChunksAroundPlayer();
		processGenerationQueue();
		rebuildDirtyMeshes();
	}

	void SetBlockSize(float s)  { blockSize = s; }
	void SetRenderDistance(int n) { renderDistance = n; }
	void SetSeed(unsigned int s) { worldSeed = s; }
	void SetTerrainParams(int base, int hill, BlockType surface, BlockType underground) {
		baseHeight = base; maxHillHeight = hill;
		surfaceType = surface; undergroundType = underground;
	}
	float GetBlockSize() const { return blockSize; }

	void SetSize(int, int) {}
	void SetOrigin(float, float) {}
	void SetMaxRenderDistance(float) {}

	void GenerateFlat(BlockType type) {
		surfaceType = type; undergroundType = type;
		baseHeight = 1; maxHillHeight = 0;
	}
	void GenerateHillyTerrain(int base, int hill, BlockType surface, BlockType underground,
							  unsigned int seed = std::random_device{}()) {
		worldSeed = seed; baseHeight = base; maxHillHeight = hill;
		surfaceType = surface; undergroundType = underground;
	}

	bool WorldToGrid(const Vector3& world, int& gx, int& gy, int& gz) const {
		gx = (int)std::floor(world.x / blockSize + 0.5f);
		gz = (int)std::floor(world.z / blockSize + 0.5f);
		gy = (int)std::floor(world.y / blockSize + 0.5f);
		return gy >= 0;
	}
	Vector3 GridToWorld(int gx, int gy, int gz) const {
		return Vector3(gx * blockSize, gy * blockSize, gz * blockSize);
	}


	bool HasBlock(int gx, int gy, int gz) const {
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);
		auto it = chunks.find({cx, cz});
		if (it == chunks.end() || !it->second->generated) return false;
		return it->second->blocks.count(Chunk::packLocal(lx, gy, lz)) > 0;
	}


	Object* GetBlock(int gx, int gy, int gz) const {
		return HasBlock(gx, gy, gz) ? reinterpret_cast<Object*>(1) : nullptr;
	}

	BlockType GetBlockType(int gx, int gy, int gz) const {
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);
		auto it = chunks.find({cx, cz});
		if (it == chunks.end()) return BlockType::Dirt;
		auto bit = it->second->blocks.find(Chunk::packLocal(lx, gy, lz));
		return (bit != it->second->blocks.end()) ? bit->second : BlockType::Dirt;
	}

	Object* CreateBlockAt(int gx, int gy, int gz, BlockType type) {
		if (gy < 0 || HasBlock(gx, gy, gz)) return nullptr;
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);
		Chunk* chunk = getOrCreateChunk(cx, cz);
		chunk->blocks[Chunk::packLocal(lx, gy, lz)] = type;
		chunk->meshDirty = true;
		markNeighborChunksDirty(gx, gz);
		return reinterpret_cast<Object*>(1);
	}

	void RemoveBlockAt(int gx, int gy, int gz) {
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);
		auto it = chunks.find({cx, cz});
		if (it == chunks.end()) return;
		int key = Chunk::packLocal(lx, gy, lz);
		if (it->second->blocks.erase(key)) {
			it->second->meshDirty = true;
			markNeighborChunksDirty(gx, gz);
		}
	}

	Object* GetBlock(int gx, int gz) const { return GetBlock(gx, 0, gz); }
	Object* CreateBlockAt(int gx, int gz, BlockType type) { return CreateBlockAt(gx, 0, gz, type); }
	void RemoveBlockAt(int gx, int gz) { RemoveBlockAt(gx, 0, gz); }

	void SetCameraObject(Object* cam) { cameraObj = cam; }

	float GetSpawnHeight(int gx, int gz) const {
		int h = getTerrainHeight(gx, gz);
		return (h + 1) * blockSize;
	}

	void ForceGenerateArea(int gx, int gz, int radiusChunks = 1) {
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);
		for (int dz = -radiusChunks; dz <= radiusChunks; ++dz)
			for (int dx = -radiusChunks; dx <= radiusChunks; ++dx)
				generateChunk(cx + dx, cz + dz);
		for (int dz = -radiusChunks; dz <= radiusChunks; ++dz)
			for (int dx = -radiusChunks; dx <= radiusChunks; ++dx) {
				auto it = chunks.find({cx + dx, cz + dz});
				if (it != chunks.end() && it->second->meshDirty)
					buildChunkMesh(it->second);
			}
	}

	void SetHighlightBlock(int gx, int gy, int gz) {
		VoxelRenderer::Get().SetHighlight(glm::vec3(gx * blockSize, gy * blockSize, gz * blockSize), true, blockSize * 0.5f);
	}
	void ClearHighlight() { 
		VoxelRenderer::Get().SetHighlight(glm::vec3(0.0f), false, blockSize * 0.5f);
	}

private:

	static void globalToChunk(int gx, int gz, int& cx, int& cz, int& lx, int& lz) {
		cx = (gx >= 0) ? (gx / CHUNK_SIZE) : ((gx - CHUNK_SIZE + 1) / CHUNK_SIZE);
		cz = (gz >= 0) ? (gz / CHUNK_SIZE) : ((gz - CHUNK_SIZE + 1) / CHUNK_SIZE);
		lx = gx - cx * CHUNK_SIZE;
		lz = gz - cz * CHUNK_SIZE;
	}

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
		float tx = fx - (float)ix;
		float tz = fz - (float)iz;
		tx = tx * tx * (3.0f - 2.0f * tx);
		tz = tz * tz * (3.0f - 2.0f * tz);
		float v00 = hashNoise(ix,     iz,     seed);
		float v10 = hashNoise(ix + 1, iz,     seed);
		float v01 = hashNoise(ix,     iz + 1, seed);
		float v11 = hashNoise(ix + 1, iz + 1, seed);
		return v00*(1-tx)*(1-tz) + v10*tx*(1-tz) + v01*(1-tx)*tz + v11*tx*tz;
	}
	int getTerrainHeight(int gx, int gz) const {
		float v1 = sampleNoise(gx, gz, 8, worldSeed);
		float v2 = sampleNoise(gx, gz, 4, worldSeed * 2u + 137u);
		float v3 = sampleNoise(gx, gz, 2, worldSeed * 3u + 5449u);
		float combined = v1 * 0.6f + v2 * 0.25f + v3 * 0.15f;
		return std::max(1, baseHeight + (int)(combined * maxHillHeight));
	}

	Chunk* getOrCreateChunk(int cx, int cz) {
		ChunkCoord cc{cx, cz};
		auto it = chunks.find(cc);
		if (it != chunks.end()) return it->second;
		Chunk* c = new Chunk();
		c->coord = cc;
		chunks[cc] = c;
		return c;
	}

	void enqueueChunk(int cx, int cz) {
		ChunkCoord cc{cx, cz};
		auto it = chunks.find(cc);
		if (it != chunks.end() && it->second->generated) return;
		for (const auto& q : generateQueue) if (q == cc) return;
		generateQueue.push_back(cc);
	}

	void processGenerationQueue() {
		if (!object || !object->GetScene()) return;
		for (int i = 0; i < CHUNKS_PER_FRAME && !generateQueue.empty(); ++i) {
			ChunkCoord cc = generateQueue.front();
			generateQueue.pop_front();
			auto it = chunks.find(cc);
			if (it != chunks.end() && it->second->generated) continue;
			generateChunk(cc.cx, cc.cz);
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
					BlockType bt = (gy == height - 1) ? surfaceType : undergroundType;
					chunk->blocks[Chunk::packLocal(lx, gy, lz)] = bt;
				}
			}
		}
		chunk->generated = true;
		chunk->meshDirty = true;

		// Adjacent chunks may need border faces updated
		static const int ddx[] = {-1, 1, 0, 0};
		static const int ddz[] = {0, 0, -1, 1};
		for (int i = 0; i < 4; ++i) {
			auto it = chunks.find({cx + ddx[i], cz + ddz[i]});
			if (it != chunks.end() && it->second->generated)
				it->second->meshDirty = true;
		}
	}

	void unloadChunk(int cx, int cz) {
		ChunkCoord cc{cx, cz};
		generateQueue.erase(
			std::remove_if(generateQueue.begin(), generateQueue.end(),
				[&](const ChunkCoord& c){ return c == cc; }),
			generateQueue.end());
		auto it = chunks.find(cc);
		if (it == chunks.end()) return;
		VoxelRenderer::Get().RemoveChunk(cx, cz);
		delete it->second;
		chunks.erase(it);
	}

	void rebuildDirtyMeshes() {
		int rebuilt = 0;
		for (auto& [cc, chunk] : chunks) {
			if (chunk->generated && chunk->meshDirty) {
				buildChunkMesh(chunk);
				if (++rebuilt >= 8) break;
			}
		}
	}

	void markNeighborChunksDirty(int gx, int gz) {
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);
		if (lx == 0)              { auto it = chunks.find({cx-1, cz}); if (it != chunks.end()) it->second->meshDirty = true; }
		if (lx == CHUNK_SIZE - 1) { auto it = chunks.find({cx+1, cz}); if (it != chunks.end()) it->second->meshDirty = true; }
		if (lz == 0)              { auto it = chunks.find({cx, cz-1}); if (it != chunks.end()) it->second->meshDirty = true; }
		if (lz == CHUNK_SIZE - 1) { auto it = chunks.find({cx, cz+1}); if (it != chunks.end()) it->second->meshDirty = true; }
	}

	void updateChunksAroundPlayer() {
		if (!object || !object->GetScene()) return;
		Vector3 camPos(0, 0, 0);
		if (cameraObj) {
			camPos = cameraObj->GetPosition3D();
		} else {
			const auto& objects = object->GetScene()->GetObjects();
			for (auto* obj : objects) {
				if (!obj) continue;
				auto* cam = obj->GetComponent<CameraComponent>();
				if (cam) { camPos = obj->GetPosition3D(); cameraObj = obj; break; }
			}
		}
		int gx, gy, gz;
		WorldToGrid(camPos, gx, gy, gz);
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);
		if (cx == lastPlayerCx && cz == lastPlayerCz) return;
		lastPlayerCx = cx; lastPlayerCz = cz;
		for (int dz = -renderDistance; dz <= renderDistance; ++dz)
			for (int dx = -renderDistance; dx <= renderDistance; ++dx) {
				auto it = chunks.find({cx + dx, cz + dz});
				if (it == chunks.end() || !it->second->generated)
					enqueueChunk(cx + dx, cz + dz);
			}
		int unloadDist = renderDistance + 2;
		std::vector<ChunkCoord> toUnload;
		for (auto& kv : chunks) {
			int ddx = kv.first.cx - cx, ddz = kv.first.cz - cz;
			if (std::abs(ddx) > unloadDist || std::abs(ddz) > unloadDist)
				toUnload.push_back(kv.first);
		}
		for (auto& cc : toUnload) unloadChunk(cc.cx, cc.cz);
	}

	//  Only exposed faces (neighbour is air) are emitted.
	//  Faces are grouped by block type so each group uses one texture.

	void buildChunkMesh(Chunk* chunk) {
		if (chunk->blocks.empty()) { 
			chunk->meshDirty = false; 
			VoxelRenderer::Get().RemoveChunk(chunk->coord.cx, chunk->coord.cz);
			return; 
		}

		std::unordered_map<int, std::vector<float>>        vertsByType;
		std::unordered_map<int, std::vector<unsigned int>>  indsByType;

		for (auto& [key, type] : chunk->blocks) {
			int lx, ly, lz;
			Chunk::unpackLocal(key, lx, ly, lz);
			int gx = chunk->coord.cx * CHUNK_SIZE + lx;
			int gz = chunk->coord.cz * CHUNK_SIZE + lz;

			float wx = gx * blockSize;
			float wy = ly * blockSize;
			float wz = gz * blockSize;
			float h  = blockSize * 0.5f;
			int t = (int)type;

			if (!hasBlockAt(gx+1, ly, gz)) appendFace(vertsByType[t], indsByType[t], wx, wy, wz, h, 0);
			if (!hasBlockAt(gx-1, ly, gz)) appendFace(vertsByType[t], indsByType[t], wx, wy, wz, h, 1);
			if (!hasBlockAt(gx, ly+1, gz)) appendFace(vertsByType[t], indsByType[t], wx, wy, wz, h, 2);
			if (ly == 0 || !hasBlockAt(gx, ly-1, gz)) appendFace(vertsByType[t], indsByType[t], wx, wy, wz, h, 3);
			if (!hasBlockAt(gx, ly, gz+1)) appendFace(vertsByType[t], indsByType[t], wx, wy, wz, h, 4);
			if (!hasBlockAt(gx, ly, gz-1)) appendFace(vertsByType[t], indsByType[t], wx, wy, wz, h, 5);
		}

		std::vector<VoxelMeshData> meshes;
		for (auto& [typeInt, verts] : vertsByType) {
			auto& inds = indsByType[typeInt];
			if (inds.empty()) continue;

			VoxelMeshData md;
			md.textureId = getTextureForType((BlockType)typeInt);
			md.vertices = std::move(verts);
			md.indices = std::move(inds);
			meshes.push_back(std::move(md));
		}

		const float chunkMaxY = 64.0f * blockSize;
		glm::vec3 aabbMin(chunk->coord.cx * CHUNK_SIZE * blockSize, 0.0f, chunk->coord.cz * CHUNK_SIZE * blockSize);
		glm::vec3 aabbMax((chunk->coord.cx + 1) * CHUNK_SIZE * blockSize, chunkMaxY, (chunk->coord.cz + 1) * CHUNK_SIZE * blockSize);

		VoxelRenderer::Get().UpdateChunk(chunk->coord.cx, chunk->coord.cz, aabbMin, aabbMax, meshes);
		chunk->meshDirty = false;
	}

	bool hasBlockAt(int gx, int gy, int gz) const {
		if (gy < 0) return true;
		int cx, cz, lx, lz;
		globalToChunk(gx, gz, cx, cz, lx, lz);
		auto it = chunks.find({cx, cz});
		if (it == chunks.end() || !it->second->generated) return false;
		return it->second->blocks.count(Chunk::packLocal(lx, gy, lz)) > 0;
	}

	// Vertex layout: pos(3) + normal(3) + uv(2) = 8 floats
	// Winding: CCW from outside (matches GL_CULL_FACE GL_BACK GL_CCW)

	static void appendFace(std::vector<float>& verts, std::vector<unsigned int>& inds,
	                        float cx, float cy, float cz, float h, int face)
	{
		unsigned int base = (unsigned int)(verts.size() / 8);
		float v[4][8];
		switch (face) {
		case 0: // +X
			v[0][0]=cx+h; v[0][1]=cy-h; v[0][2]=cz-h; v[0][3]= 1; v[0][4]=0; v[0][5]=0; v[0][6]=0; v[0][7]=0;
			v[1][0]=cx+h; v[1][1]=cy+h; v[1][2]=cz-h; v[1][3]= 1; v[1][4]=0; v[1][5]=0; v[1][6]=0; v[1][7]=1;
			v[2][0]=cx+h; v[2][1]=cy+h; v[2][2]=cz+h; v[2][3]= 1; v[2][4]=0; v[2][5]=0; v[2][6]=1; v[2][7]=1;
			v[3][0]=cx+h; v[3][1]=cy-h; v[3][2]=cz+h; v[3][3]= 1; v[3][4]=0; v[3][5]=0; v[3][6]=1; v[3][7]=0;
			break;
		case 1: // -X
			v[0][0]=cx-h; v[0][1]=cy-h; v[0][2]=cz+h; v[0][3]=-1; v[0][4]=0; v[0][5]=0; v[0][6]=0; v[0][7]=0;
			v[1][0]=cx-h; v[1][1]=cy+h; v[1][2]=cz+h; v[1][3]=-1; v[1][4]=0; v[1][5]=0; v[1][6]=0; v[1][7]=1;
			v[2][0]=cx-h; v[2][1]=cy+h; v[2][2]=cz-h; v[2][3]=-1; v[2][4]=0; v[2][5]=0; v[2][6]=1; v[2][7]=1;
			v[3][0]=cx-h; v[3][1]=cy-h; v[3][2]=cz-h; v[3][3]=-1; v[3][4]=0; v[3][5]=0; v[3][6]=1; v[3][7]=0;
			break;
		case 2: // +Y
			v[0][0]=cx-h; v[0][1]=cy+h; v[0][2]=cz-h; v[0][3]=0; v[0][4]= 1; v[0][5]=0; v[0][6]=0; v[0][7]=0;
			v[1][0]=cx-h; v[1][1]=cy+h; v[1][2]=cz+h; v[1][3]=0; v[1][4]= 1; v[1][5]=0; v[1][6]=0; v[1][7]=1;
			v[2][0]=cx+h; v[2][1]=cy+h; v[2][2]=cz+h; v[2][3]=0; v[2][4]= 1; v[2][5]=0; v[2][6]=1; v[2][7]=1;
			v[3][0]=cx+h; v[3][1]=cy+h; v[3][2]=cz-h; v[3][3]=0; v[3][4]= 1; v[3][5]=0; v[3][6]=1; v[3][7]=0;
			break;
		case 3: // -Y
			v[0][0]=cx-h; v[0][1]=cy-h; v[0][2]=cz+h; v[0][3]=0; v[0][4]=-1; v[0][5]=0; v[0][6]=0; v[0][7]=0;
			v[1][0]=cx-h; v[1][1]=cy-h; v[1][2]=cz-h; v[1][3]=0; v[1][4]=-1; v[1][5]=0; v[1][6]=0; v[1][7]=1;
			v[2][0]=cx+h; v[2][1]=cy-h; v[2][2]=cz-h; v[2][3]=0; v[2][4]=-1; v[2][5]=0; v[2][6]=1; v[2][7]=1;
			v[3][0]=cx+h; v[3][1]=cy-h; v[3][2]=cz+h; v[3][3]=0; v[3][4]=-1; v[3][5]=0; v[3][6]=1; v[3][7]=0;
			break;
		case 4: // +Z
			v[0][0]=cx-h; v[0][1]=cy-h; v[0][2]=cz+h; v[0][3]=0; v[0][4]=0; v[0][5]= 1; v[0][6]=0; v[0][7]=0;
			v[1][0]=cx+h; v[1][1]=cy-h; v[1][2]=cz+h; v[1][3]=0; v[1][4]=0; v[1][5]= 1; v[1][6]=1; v[1][7]=0;
			v[2][0]=cx+h; v[2][1]=cy+h; v[2][2]=cz+h; v[2][3]=0; v[2][4]=0; v[2][5]= 1; v[2][6]=1; v[2][7]=1;
			v[3][0]=cx-h; v[3][1]=cy+h; v[3][2]=cz+h; v[3][3]=0; v[3][4]=0; v[3][5]= 1; v[3][6]=0; v[3][7]=1;
			break;
		case 5: // -Z
			v[0][0]=cx+h; v[0][1]=cy-h; v[0][2]=cz-h; v[0][3]=0; v[0][4]=0; v[0][5]=-1; v[0][6]=0; v[0][7]=0;
			v[1][0]=cx-h; v[1][1]=cy-h; v[1][2]=cz-h; v[1][3]=0; v[1][4]=0; v[1][5]=-1; v[1][6]=1; v[1][7]=0;
			v[2][0]=cx-h; v[2][1]=cy+h; v[2][2]=cz-h; v[2][3]=0; v[2][4]=0; v[2][5]=-1; v[2][6]=1; v[2][7]=1;
			v[3][0]=cx+h; v[3][1]=cy+h; v[3][2]=cz-h; v[3][3]=0; v[3][4]=0; v[3][5]=-1; v[3][6]=0; v[3][7]=1;
			break;
		}
		for (int i = 0; i < 4; ++i)
			for (int j = 0; j < 8; ++j)
				verts.push_back(v[i][j]);
		inds.push_back(base);     inds.push_back(base + 1); inds.push_back(base + 2);
		inds.push_back(base);     inds.push_back(base + 2); inds.push_back(base + 3);
	}

	void preloadTextures() {
		auto& rm = ResourceManager::Get();
		texDirt  = rm.LoadTexture("Assets/block_textures/dirt.png");
		texStone = rm.LoadTexture("Assets/block_textures/stone.png");
		texGrass = rm.LoadTexture("Assets/block_textures/grass.png");
		texSand  = rm.LoadTexture("Assets/block_textures/sand.png");
		texWood  = rm.LoadTexture("Assets/block_textures/wood.png");
	}

	GLuint getTextureForType(BlockType t) const {
		switch (t) {
			case BlockType::Dirt:  return texDirt;
			case BlockType::Stone: return texStone;
			case BlockType::Grass: return texGrass;
			case BlockType::Sand:  return texSand;
			case BlockType::Wood:  return texWood;
		}
		return texDirt;
	}

private:
	float blockSize;
	int renderDistance;
	unsigned int worldSeed;
	int baseHeight, maxHillHeight;
	BlockType surfaceType, undergroundType;

	std::unordered_map<ChunkCoord, Chunk*, ChunkCoordHash> chunks;
	std::deque<ChunkCoord> generateQueue;
	static constexpr int CHUNKS_PER_FRAME = 8;

	Object* cameraObj = nullptr;
	int lastPlayerCx, lastPlayerCz;

	GLuint texDirt = 0, texStone = 0, texGrass = 0, texSand = 0, texWood = 0;
};
