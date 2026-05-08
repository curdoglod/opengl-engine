#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Shared render state used by components.
class Renderer {
public:
    static Renderer& Get() {
        static Renderer instance;
        return instance;
    }

    void SetWindowSize(int w, int h) {
        windowWidth  = w;
        windowHeight = h;
    }

    int GetWindowWidth()  const { return windowWidth; }
    int GetWindowHeight() const { return windowHeight; }

    glm::mat4 GetOrthoProjection() const {
        return glm::ortho(0.0f,
                          static_cast<float>(windowWidth),
                          static_cast<float>(windowHeight),
                          0.0f,
                          -1.0f, 1.0f);
    }

private:
    Renderer() = default;
    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    int windowWidth  = 800;
    int windowHeight = 480;
};
