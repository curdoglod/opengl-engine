#include "SceneDeclarations.h"

void GameScene::Init()
{
    block_size = 40;
    block_count = GetWindowSize()/block_size; 
    GenBackground();

    snake = CreateObject();
    snake->AddComponent(new SnakeComponent(block_size));
    apple = CreateObject();
    apple->SetLayer(5);
    snake->SetLayer(10);
    std::vector<unsigned char> appleImg = Engine::GetResourcesArchive()->GetFile("apple.png");

    apple->AddComponent(new Image(appleImg));
    apple->SetPosition(RandomApple()); 
    while (apple->Crossing(snake))
    {
        apple->SetPosition(RandomApple());
    }
    appleCount=0; 
    scoreObj = CreateObject(); 
    scoreObj->SetLayer(100); 
    scoreObj->SetPosition(Vector2(block_size/5, block_size/2));
    scoreObj->AddComponent(new TextComponent(20, "Score: " + std::to_string(appleCount), TextAlignment::LEFT));
}

void GameScene::Update()
{
    if (apple->Crossing(snake, 0.5,0.5))
    {
        while (apple->Crossing(snake))
        {
            apple->SetPosition(RandomApple());
            appleCount++;
            snake->GetComponent<SnakeComponent>()->AddSegment();
            scoreObj->GetComponent<TextComponent>()->setText("Score: " + std::to_string(appleCount));
        }
    }
}

void GameScene::GenBackground()
{
    std::vector<unsigned char> block_light_img = Engine::GetResourcesArchive()->GetFile("block_sgreen.png");
    std::vector<unsigned char> block_dark_img = Engine::GetResourcesArchive()->GetFile("block_tgreen.png");

    for (int j = 0; j < block_count.y; j++)
        for (int i = 0; i < block_count.x; i++)
        {
            Object *block_background = CreateObject();
            block_background->SetPosition(Vector2(i * block_size, j * block_size));
            if ((j==0)||(i % 2 == 1 && j % 2 == 0) || (i % 2 == 0 && j % 2 == 1))
            {
                block_background->AddComponent(new Image(block_light_img));
            }
            else
            {
                block_background->AddComponent(new Image(block_dark_img));
            }
        }
}

Vector2 GameScene::RandomApple()
{
    return Vector2(rand() % (int)block_count.x * block_size, rand() % (int)block_count.y * block_size); 
}
