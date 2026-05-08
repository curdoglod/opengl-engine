#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <SDL.h>
#include "Utils.h"

class Object;
class Engine;
class Sprite;

class Scene {
public:
    virtual ~Scene();

    void PreInit(Engine* engine, SDL_Window* window);

    virtual void Awake() {}
    virtual void Init() {}
    virtual void Update() {}

    void UpdateEvents(SDL_Event event);
    void UpdateScene(float deltaTime);

    Object* CreateObject();
    void DeleteObject(Object* object);
    void flushPendingDeletes();

    Sprite* createSprite(const std::vector<unsigned char>& imageData);

    void updateLayer();

    Vector2 GetWindowSize();

    void SwitchToScene(Scene* newScene);
    void PushScene(Scene* scene);
    void PopScene();

    void SetWindowSize(const int& w, const int& h);

    virtual void onMouseMove(int x, int y) {}
    virtual void onMouseButtonDown(Uint8 button, int x, int y) {}
    virtual void onMouseButtonUp(Uint8 button, int x, int y) {}
    virtual void onKeyPressed(SDL_Keycode key) {}
    virtual void onKeyReleased(SDL_Keycode key) {}

    const std::vector<Object*>& GetObjects() const { return objects; }

private:
    void dispatchCollisions();

private: 
    std::vector<Object*> objects;
    std::vector<Object*> pendingDeletes;
    bool layerDirty = false;
    SDL_Window* m_window = nullptr;
    Engine* m_engine = nullptr;
};

#endif // SCENE_H

