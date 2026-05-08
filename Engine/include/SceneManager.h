#ifndef SCENEMANAGER_H
#define SCENEMANAGER_H

#include <stack>
#include <vector>
#include <memory>

class Scene;
class Engine;
struct SDL_Window;

// Stack of scenes. Old scenes are deleted on the next frame so callbacks can
// switch scenes safely.
class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    void Bind(Engine* engine, SDL_Window* window);

    void ReplaceScene(Scene* scene);
    void PushScene(Scene* scene);
    void PopScene();

    Scene* GetActiveScene() const;
    bool HasScene() const;
    std::size_t SceneCount() const;

    void FlushPending();

private:
    void initScene(Scene* scene);
    void clearStack();

    std::stack<Scene*>   m_scenes;
    std::vector<Scene*>  m_pendingDelete;
    Engine*              m_engine = nullptr;
    SDL_Window*          m_window = nullptr;
};

#endif // SCENEMANAGER_H

