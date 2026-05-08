#include "SceneDeclarations.h"

void Arkanoid3DScene::Init()
{
    Vector2 windowSize(800, 480);
    SetWindowSize(windowSize.x, windowSize.y);

    cameraObj = CreateObject();
    cameraObj->AddComponent(new CameraComponent());
    auto* cam = cameraObj->GetComponent<CameraComponent>();
    cam->SetPerspective(60.0f, windowSize.x / windowSize.y, 0.1f, 100.0f);
    cameraObj->SetPosition(Vector3(0.0f, 120.0f / 35.0f, 78.0f / 35.0f));
    cameraObj->SetRotation(Vector3(30.0f, 0.0f, 0.0f));

    lightObj = CreateObject();
    auto* light = new LightComponent();
    light->SetDirection(glm::vec3(0.2f, -1.0f, 0.1f));
    light->SetColor(glm::vec3(1.0f, 1.0f, 1.0f));
    light->SetAmbient(glm::vec3(0.20f, 0.20f, 0.20f));
    light->SetShadowEnabled(true);
    light->SetShadowMapSize(64, 64);
    lightObj->AddComponent(light);

    board = CreateObject();
    board->AddComponent(new Model3DComponent("Assets/board.fbx"));
    board->GetComponent<Model3DComponent>()->SetAlbedoTextureFromFile("Assets/block_textures/grass.png");
    board->SetPosition(Vector3(0.0f, -1.5f / 35.0f, 0.0f));
    board->SetSize(Vector3(2,1,1) / 200.0f);
    board->SetRotation(Vector3(0.0f, 90.0f, 0.0f));
    board->SetLayer(100);
    auto* boardCol = new BoxCollider3D();
    boardCol->SetTrigger(true); 
    board->AddComponent(boardCol);

    ball = CreateObject();
    ball->AddComponent(new Model3DComponent("Assets/ball.fbx"));
    ball->SetPosition(Vector3(0.0f, 30.0f / 35.0f, 0.0f));
    ball->SetSize(Vector3(1.0f, 1.0f, 1.0f) / 100.0f);
    ball->SetRotation(Vector3(-90.0f, 0.0f, 0.0f));
    ball->SetLayer(120);
    ball->AddComponent(new BoxCollider3D());
    auto* rb = new Rigidbody3D();
    rb->SetGravity(-29.81f / 35.0f);
    rb->SetVelocity(Vector3(0, 0, 0));
    ball->AddComponent(rb);
}

void Arkanoid3DScene::Update()
{
    auto* ballCol = ball->GetComponent<BoxCollider3D>();
    auto* boardCol = board->GetComponent<BoxCollider3D>();
    auto* rb = ball->GetComponent<Rigidbody3D>();
    if (ballCol && boardCol && rb) {
        ballCol->AutoFitFromModel();
        boardCol->AutoFitFromModel();
        if (ballCol->Overlaps(boardCol)) {
            Vector3 vel = rb->GetVelocity();
            if (vel.y < 0) {
                vel.y = 40.0f / 35.0f;
                rb->SetVelocity(vel);
                Vector3 pos = ball->GetPosition3D();
                Vector3 bPos = board->GetPosition3D();
                Vector3 heBall = ballCol->GetHalfExtents();
                Vector3 heBoard = boardCol->GetHalfExtents();
                float topOfBoard = bPos.y + heBoard.y;
                pos.y = topOfBoard + heBall.y + 0.1f;
                ball->SetPosition(pos);
            }
        }
    }
}


void Arkanoid3DScene::onKeyPressed(SDL_Keycode key)
{
    int k = 4;
    float step = k / 35.0f;
    if (key == SDLK_w) {
        board->SetPosition(board->GetPosition3D()+Vector3(0,0,-1)*step);
    }
    if (key == SDLK_s) {
        board->SetPosition(board->GetPosition3D()+Vector3(0,0,1)*step);
    }
    if (key == SDLK_a) {
        board->SetPosition(board->GetPosition3D()+Vector3(-1,0,0)*step);
    }
    if (key == SDLK_d) {
        board->SetPosition(board->GetPosition3D()+Vector3(1,0,0)*step);
    }
}
