#pragma once
#include "component.h"
#include "object.h"
#include "Scene.h"
#include "engine.h"
#include "image.h"
#include <iostream>
#include <cmath>

class PaddleComponent : public Component
{
public:
  
    void Init() override
    {
        windowSize = object->GetScene()->GetWindowSize();
        std::vector<unsigned char> paddleImgData = Engine::GetResourcesArchive()->GetFile("paddle.png");
        object->AddComponent(new Image(paddleImgData));
        object->SetPosition(Vector2(windowSize.x/2, windowSize.y*0.9f));
    }

    void onKeyPressed(SDL_Keycode key) override
    {

        if (key == SDLK_LEFT || key == SDLK_a)
        {
            moveDirection = -1;
        }
        else if (key == SDLK_RIGHT || key == SDLK_d)
        {
            moveDirection = 1;
        }
        else if (key == SDLK_SPACE)
        {
            object->GetScene()->SwitchToScene(new MainGameScene());
        }
    }

    void onKeyReleased(SDL_Keycode key) override
    {
        if (((key == SDLK_RIGHT|| key == SDLK_d ) && moveDirection == 1) || ((key == SDLK_LEFT || key == SDLK_a) && moveDirection == -1))
        {
            moveDirection = 0;
        }
    }

    void Update() override
    {
        if (object != nullptr && moveDirection != 0)
        {
            object->MoveX(moveDirection * 20);
            if (object->GetPosition().x <= 0)
                object->SetPosition(Vector2(0, object->GetPosition().y));
            else if (object->GetPosition().x + object->GetSize().x >= windowSize.x)
                object->SetPosition(Vector2(windowSize.x - object->GetSize().x, object->GetPosition().y));
        }
    }

private:
    Vector2 windowSize;
    int moveDirection = 0;
};
