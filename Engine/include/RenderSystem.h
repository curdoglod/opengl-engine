#pragma once

class Scene;

// One render pass for shadows, 3D models, sprites, and text.
class RenderSystem {
public:
    static void Render(Scene* scene);
};
