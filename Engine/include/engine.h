#ifndef ENGINE_H
#define ENGINE_H

#include "iostream"

class Scene;
class ArchiveUnpacker;

class Engine {
public:
    Engine();
    ~Engine();

    virtual void Init() = 0;

    void Run();

    void ChangeScene(Scene* newScene);

    void PushScene(Scene* scene);

    void PopScene();

    static ArchiveUnpacker* GetDefaultArchive();
    static ArchiveUnpacker* GetResourcesArchive();

    void SetWindowSize(const int& w, const int& h);

    void SetWindowTitle(const std::string& newTitle);

    void Quit();

    void SetFPS(const int& fps);

private:
    class Impl;
    Impl* impl;
};

#endif // ENGINE_H
