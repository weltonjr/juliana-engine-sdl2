#include "raylib.h"
#include "Game.h"
#include "core/Types.h"

int main() {
    const int screen_w = 1024;
    const int screen_h = 768;

    InitWindow(screen_w, screen_h, "Aeterium v0.1");
    SetTargetFPS(60);

    Game game(screen_w, screen_h);

    float accumulator = 0.0f;

    while (!WindowShouldClose()) {
        float frame_time = GetFrameTime();
        if (frame_time > 0.05f) frame_time = 0.05f; // cap at 50ms

        accumulator += frame_time;
        while (accumulator >= PHYSICS_DT) {
            game.update(PHYSICS_DT);
            accumulator -= PHYSICS_DT;
        }

        BeginDrawing();
        game.draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
