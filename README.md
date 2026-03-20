# 🎮 OpenGL Game Engine

A lightweight **C++ game engine** built on **OpenGL 3.3 Core** and **SDL2**, featuring 2D/3D rendering, shadow mapping, voxel terrain, and a component-based architecture — all packed with several playable mini-games.

---

## ✨ Features

### Rendering Pipeline
- **OpenGL 3.3 Core** profile with depth testing, back-face culling, and alpha blending
- **Shadow Mapping** — directional light with depth FBO, 5×5 PCF soft shadows, slope-scaled bias, and border fade
- **Frustum Culling** — sphere tests for 3D models, AABB tests for voxel chunks
- **Lambert Lighting** — diffuse + ambient shading for models and voxels
- **Voxel Optimization** — chunk-based meshing with only exposed faces rendered

### Engine Architecture
- **Component System** — objects composed of Camera, Light, Model3D, Sprite, Text, Rigidbody, Collider, and more
- **Scene Manager** — push/pop/change scene stack for menus and game states
- **Resource Manager** — shared shader, texture, and mesh caching (no duplicate Assimp loads)
- **Asset Packer** — archives for `DefaultAssets/` and `Assets/` loaded into memory at startup
- **2D & 3D Physics** — Rigidbody2D/3D and BoxCollider3D with collision callbacks

### Shader Programs (GLSL 330)
| Shader | Purpose |
|---|---|
| `sprite` | 2D textured quads with orthographic projection |
| `text` | Font rendering via SDL2_ttf textures |
| `model3d` | 3D models with Lambert shading, shadows, and highlight tint |
| `chunk_mesh` | Voxel chunks with lighting, shadows, and block highlight |
| `depth` | Shadow map depth-only pass |

---

## 🕹️ Included Games

| Game | Description |
|---|---|
| **Snake** | Classic grid-based snake with apple spawning and score tracking |
| **Arkanoid 2D** | Paddle and bouncing ball, destroy all the blocks |
| **Arkanoid 3D** | Full 3D board with Rigidbody3D physics and WASD paddle control |
| **Minecraft Clone** | Procedural voxel terrain (16×16 chunks), block place/destroy, first-person camera, jump & gravity, hotbar, raycast selection |

---

## 🔧 Dependencies

| Library | Purpose |
|---|---|
| [SDL2](https://www.libsdl.org/) | Window management, input, events |
| [SDL2_ttf](https://wiki.libsdl.org/SDL2_ttf) | TrueType font rendering |
| [SDL2_image](https://wiki.libsdl.org/SDL2_image) | Image loading (PNG, JPG, etc.) |
| [GLEW](http://glew.sourceforge.net/) | OpenGL extension loading |
| [GLM](https://github.com/g-truc/glm) | Mathematics (vectors, matrices, transforms) |
| [Assimp](https://www.assimp.org/) | 3D model import (FBX, OBJ) |

### Install (macOS)

```bash
brew install sdl2 sdl2_ttf sdl2_image glew glm assimp
```

### Install (Ubuntu / Debian)

```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libglew-dev libglm-dev libassimp-dev
```

---

## 🚀 Build & Run

```bash
# Build everything (Engine + Game)
make all

# Build and launch
make run

# Rebuild from scratch
make re

# Clean build artifacts
make clean_all
```

The build produces a static library `libEngine.a` and links it into the final `game_app` executable.

| Platform | Compiler | GL Loader |
|---|---|---|
| macOS | `clang++` | `-framework OpenGL` |
| Linux | `g++` | `-lGLEW -lGL` |

---

## 📁 Project Structure

```
opengl-2/
├── Engine/
│   ├── include/          # Engine headers
│   │   ├── engine.h              # Core loop, SDL/GL init
│   │   ├── RenderSystem.h        # Shadow → 3D → 2D render pipeline
│   │   ├── ResourceManager.h     # Shader/texture/mesh cache
│   │   ├── Scene.h               # Object lifecycle & collision
│   │   ├── SceneManager.h        # Scene stack management
│   │   ├── CameraComponent.h     # Perspective camera
│   │   ├── LightComponent.h      # Directional light & shadows
│   │   ├── Model3DComponent.h    # 3D model rendering
│   │   ├── VoxelRenderer.h       # Chunk-based voxel meshes
│   │   ├── Frustum.h             # Gribb–Hartmann frustum culling
│   │   ├── Rigidbody3D.h         # 3D physics body
│   │   ├── BoxCollider3D.h       # 3D box collision
│   │   └── ...
│   ├── src/              # Engine implementation
│   └── Makefile
├── Game/
│   ├── include/          # Game-specific headers
│   ├── src/              # Game scenes & logic
│   └── Makefile
├── Assets/               # Textures, models, block atlases
├── DefaultAssets/        # Fonts (Roboto), fallback images
└── Makefile              # Root build orchestrator
```

---

## 🏗️ Technical Highlights

- **Voxel meshing** — only exposed block faces are generated, reducing draw calls from thousands to tens per frame
- **Shadow texel snapping** — eliminates shadow swimming artifacts during camera movement
- **Distance culling** — objects beyond threshold are skipped in the shadow depth pass
- **Shared mesh cache** — identical FBX/OBJ models loaded once and reused across all components
- **Embedded GLSL** — all shaders compiled from strings at runtime, no external `.glsl` files needed

---

## 📝 License

This project is for educational purposes.
