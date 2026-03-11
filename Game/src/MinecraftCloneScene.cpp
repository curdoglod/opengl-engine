#include "SceneDeclarations.h"

void MinecraftCloneScene::Init()
{
    Vector2 windowSize(1280, 720);
    SetWindowSize(windowSize.x, windowSize.y);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    camObj = CreateObject();
    camObj->AddComponent(new CameraComponent());
    auto *cam = camObj->GetComponent<CameraComponent>();
    cam->SetPerspective(70.0f, windowSize.x / windowSize.y, 0.05f, 1000.0f);

    lightObj = CreateObject();
    auto *light = new LightComponent();
    light->SetDirection(glm::vec3(0.3f, -1.0f, 0.2f));
    light->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
    light->SetAmbient(glm::vec3(0.25f, 0.25f, 0.25f));
    light->SetShadowEnabled(true);
    light->SetShadowMapSize(4096, 4096);
    lightObj->AddComponent(light);

    world = CreateObject();
    grid = new WorldGridComponent();
    grid->SetBlockSize(20.0f / 35.0f);
    grid->SetRenderDistance(10);
    grid->SetCameraObject(camObj);
    grid->SetTerrainParams(3, 6, BlockType::Dirt, BlockType::Stone);
    world->AddComponent(grid);
    // Generate spawn chunks upfront to avoid falling through not-yet-loaded terrain.
    grid->ForceGenerateArea(0, 0);

    Object *player = CreateObject();
    float spawnY = grid->GetSpawnHeight(0, 0);
    player->SetPosition(Vector3(0.0f, spawnY, 0.0f));
    auto *pc = new PlayerController();
    pc->SetMoveSpeed(160.0f / 35.0f);
    pc->SetCamera(camObj);
    pc->SetEyeHeight(25.0f / 35.0f);
    pc->SetGravity(-600.0f / 35.0f);
    pc->SetJumpSpeed(220.0f / 35.0f);
    pc->SetMouseSensitivity(0.20f);

    player->AddComponent(pc);

    Object *crosshair = CreateObject();
    crosshair->SetPosition(Vector2(windowSize.x * 0.5f, windowSize.y * 0.5f));
    crosshair->SetLayer(1000);
    crosshair->AddComponent(new Image(Engine::GetResourcesArchive()->GetFile("ball.png")));
    crosshair->SetSize(Vector2(8, 8));

    hotbarObj = CreateObject();
    hotbarObj->SetLayer(1000);
    hotbarObj->SetPosition(Vector2(windowSize.x * 0.5f,
                                   windowSize.y - 50.0f));

    hotbar = new HotbarComponent();
    hotbarObj->AddComponent(hotbar);
    pc->SetHotbar(hotbar);
}

void MinecraftCloneScene::Update()
{
}