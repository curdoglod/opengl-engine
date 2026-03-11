#include "engine.h"
#include "SceneDeclarations.h"







class Game : public Engine {
public:
    void Init() override {
        SetWindowTitle("Game");
        SetWindowSize(400, 700);
        ChangeScene(new StartScene());
    }
};





int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    Game game;
    game.Run();

    return 0;
}

