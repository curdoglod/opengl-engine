#include "SceneManager.h"
#include "Scene.h"
#include "engine.h"

SceneManager::~SceneManager() {
    clearStack();
    FlushPending();
}

void SceneManager::Bind(Engine* engine, SDL_Window* window) {
    m_engine = engine;
    m_window = window;
}

void SceneManager::ReplaceScene(Scene* scene) {
    // Scene switches can happen inside callbacks, so delete old scenes next frame.
    while (!m_scenes.empty()) {
        m_pendingDelete.push_back(m_scenes.top());
        m_scenes.pop();
    }
    initScene(scene);
    m_scenes.push(scene);
}

void SceneManager::PushScene(Scene* scene) {
    initScene(scene);
    m_scenes.push(scene);
}

void SceneManager::PopScene() {
    if (m_scenes.size() <= 1)
        return;

    m_pendingDelete.push_back(m_scenes.top());
    m_scenes.pop();
}

Scene* SceneManager::GetActiveScene() const {
    return m_scenes.empty() ? nullptr : m_scenes.top();
}

bool SceneManager::HasScene() const {
    return !m_scenes.empty();
}

std::size_t SceneManager::SceneCount() const {
    return m_scenes.size();
}

void SceneManager::FlushPending() {
    for (Scene* s : m_pendingDelete)
        delete s;
    m_pendingDelete.clear();
}

void SceneManager::initScene(Scene* scene) {
    scene->PreInit(m_engine, m_window);
    scene->Awake();
    scene->Init();
}

void SceneManager::clearStack() {
    while (!m_scenes.empty()) {
        delete m_scenes.top();
        m_scenes.pop();
    }
}
