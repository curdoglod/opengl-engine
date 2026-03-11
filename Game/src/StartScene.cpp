#include "SceneDeclarations.h"
#include <functional>

namespace
{
Object *CreateMenuButton(StartScene *scene,
                         Object *templateButton,
                         const Vector2 &baseSize,
                         float yOffset,
                         const std::string &title,
                         std::function<void()> onClick)
{
    Object *button = templateButton->CloneObject();
    button->MoveY(baseSize.y * yOffset);
    button->GetComponent<ButtonComponent>()->SetOnClick(std::move(onClick));
    button->GetComponent<TextComponent>()->setText(title);
    return button;
}
}

void StartScene::Awake()
{
}

void StartScene::Init()
{
    my3DObject = CreateObject();
    my3DObject->AddComponent(new Model3DComponent("Assets/model.fbx"));
    my3DObject->SetPosition(Vector3(50.0f / 35.0f, 10.0f / 35.0f, 100.0f / 35.0f));
    my3DObject->SetRotation(Vector3(-90, 0, 0));
    my3DObject->SetLayer(200);
    Vector2 windowSize(1280, 720);
    SetWindowSize(windowSize.x, windowSize.y);
    glViewport(0, 0, windowSize.x, windowSize.y);
    UIdraw();
}
void StartScene::UIdraw()
{
    Vector2 windowSize(1280, 720);

    Object *background = CreateObject();
    background->AddComponent(new Image(Engine::GetResourcesArchive()->GetFile("block_sgreen.png")));
    background->GetComponent<Image>()->SetSize(windowSize);
    start_button = CreateObject();
    start_button->AddComponent(new ButtonComponent([this]()
                                                   { SwitchToScene(new GameScene()); }));
    startBttn_image = start_button->GetComponent<Image>();
    startBttn_image->SetNewSprite(Engine::GetResourcesArchive()->GetFile("block_tgreen.png"));
    startBttn_image->SetSize(Vector2(150, 50));
    start_button->SetPosition(GetWindowSize() / 2 - startBttn_image->GetSize() / 2);
    start_button->AddComponent(new TextComponent(20, "Snake Game", Color(255, 255, 255), TextAlignment::CENTER));

    const Vector2 buttonSize = startBttn_image->GetSize();
    CreateMenuButton(this,
                     start_button,
                     buttonSize,
                     1.2f,
                     "Arkanoid Game",
                     [this]() { SwitchToScene(new MainGameScene()); });

    CreateMenuButton(this,
                     start_button,
                     buttonSize,
                     2.4f,
                     "Minecraft Clone",
                     [this]() { SwitchToScene(new MinecraftCloneScene()); });
}
void StartScene::Update()
{
    my3DObject->SetRotation(my3DObject->GetAngle() + Vector3(1, 1, 1));
}
