#pragma once
#include "component.h"
#include "object.h"
#include "Model3DComponent.h"
#include "BoxCollider3D.h"

enum class BlockType {
	Dirt,
	Stone,
	Grass,
	Sand,
	Wood
};

class BlockComponent : public Component {
public:
	BlockComponent() : type(BlockType::Dirt) {}

	void Init() override {
		if (!object->GetComponent<Model3DComponent>()) {
			object->AddComponent(new Model3DComponent("Assets/cube.fbx"));
		}

		if (object->GetSize3D() == Vector3(0,0,0)) {
			object->SetSize(Vector3(1,1,1));
		}
		applyTypeTexture();
	}

	void SetType(BlockType t) { type = t; applyTypeTexture(); }
	BlockType GetType() const { return type; }

	Component* Clone() const override { return new BlockComponent(*this); }

private:
	void applyTypeTexture() {
		Model3DComponent* model = object->GetComponent<Model3DComponent>();
		if (!model) return;
		std::string path;
		switch (type) {
			case BlockType::Dirt:  path = "Assets/block_textures/dirt.png"; break;
			case BlockType::Stone: path = "Assets/block_textures/stone.png"; break;
			case BlockType::Grass: path = "Assets/block_textures/grass.png"; break;
			case BlockType::Sand:  path = "Assets/block_textures/sand.png"; break;
			case BlockType::Wood:  path = "Assets/block_textures/wood.png"; break;
		}
		model->SetAlbedoTextureFromFile(path);
	}

	BlockType type;
};
