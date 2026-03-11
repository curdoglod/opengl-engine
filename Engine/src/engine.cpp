#include "engine.h"
#include <iostream>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <algorithm>
#include "Scene.h"
#include "SceneManager.h"
#include "Renderer.h"
#include "InputManager.h"
#include "ResourceManager.h"
#include "ArchiveUnpacker.h"
#include "RenderSystem.h"
#include <chrono>
#include <GL/glew.h>

struct Engine::Impl
{
    Impl() {}
    void preInit()
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

        m_window = SDL_CreateWindow("Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        SDL_GLContext glContext = SDL_GL_CreateContext(m_window);
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK)
        {
            std::cerr << "GLEW initialization error!" << std::endl;
        }
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    }

    void Tick(float deltaTime)
    {
        sceneManager.FlushPending();

        int ww, wh;
        SDL_GetWindowSize(m_window, &ww, &wh);
        Renderer::Get().SetWindowSize(ww, wh);
        glViewport(0, 0, ww, wh);

        InputManager::Get().BeginFrame();

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            InputManager::Get().ProcessEvent(event);

            if (event.type == SDL_QUIT)
            {
                SDL_DestroyWindow(m_window);
                m_window = nullptr;
            }

            Scene *active = sceneManager.GetActiveScene();
            if (active != nullptr)
                active->UpdateEvents(event);
        }

        glClearColor(0, 100 / 255.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Scene *active = sceneManager.GetActiveScene();
        if (active != nullptr)
            active->UpdateScene(deltaTime);

        active = sceneManager.GetActiveScene();
        if (active != nullptr)
            RenderSystem::Render(active);

        SDL_GL_SwapWindow(m_window);
    }

    int FPS = 60;
    SDL_Window *m_window = nullptr;
    std::string nameWindow;

    SceneManager sceneManager;

    static ArchiveUnpacker *DefaultArchive;
    static ArchiveUnpacker *ResourcesArchive;
};

ArchiveUnpacker *Engine::Impl::DefaultArchive = nullptr;
ArchiveUnpacker *Engine::Impl::ResourcesArchive = nullptr;

Engine::Engine() : impl(new Impl())
{
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    impl->DefaultArchive = new ArchiveUnpacker("DefaultAssets");
    impl->ResourcesArchive = new ArchiveUnpacker("Assets");
    impl->DefaultArchive->Unpack();
    impl->ResourcesArchive->Unpack();
    impl->FPS = 60;
}

void Engine::Run()
{
    impl->preInit();
    impl->sceneManager.Bind(this, impl->m_window);

    Init();

    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    int frameDelay = 1000 / impl->FPS;

    while (impl->m_window != nullptr)
    {
        auto frameStart = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> frameDuration = frameStart - lastFrameTime;
        lastFrameTime = frameStart;

        impl->Tick(frameDuration.count() / 1000.0f);

        auto frameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> elapsedTime = frameEnd - frameStart;
        int elapsedMS = static_cast<int>(elapsedTime.count());

        if (frameDelay > elapsedMS)
        {
            SDL_Delay(frameDelay - elapsedMS);
        }
    }
}

void Engine::ChangeScene(Scene *newScene)
{
    impl->sceneManager.ReplaceScene(newScene);
}

void Engine::PushScene(Scene *scene)
{
    impl->sceneManager.PushScene(scene);
}

void Engine::PopScene()
{
    impl->sceneManager.PopScene();
}

ArchiveUnpacker *Engine::GetDefaultArchive()
{
    return Engine::Impl::DefaultArchive;
}

ArchiveUnpacker *Engine::GetResourcesArchive()
{
    return Engine::Impl::ResourcesArchive;
}

void Engine::SetFPS(const int &fps) { impl->FPS = fps; };

void Engine::SetWindowSize(const int &w, const int &h)
{
    SDL_SetWindowSize(impl->m_window, w, h);
    Renderer::Get().SetWindowSize(w, h);
}

void Engine::SetWindowTitle(const std::string &newTitle)
{
    SDL_SetWindowTitle(impl->m_window, newTitle.c_str());
}

void Engine::Quit()
{
    SDL_DestroyWindow(impl->m_window);
    IMG_Quit();
    SDL_Quit();
}

Engine::~Engine()
{
    ResourceManager::Get().ReleaseAll();
    SDL_DestroyWindow(impl->m_window);
    IMG_Quit();
    SDL_Quit();
    delete impl->DefaultArchive;
    delete impl->ResourcesArchive;
    delete impl;
}
