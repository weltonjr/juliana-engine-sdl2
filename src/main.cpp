#include "raylib.h"

int main()
{
    const int screen_width = 1024;
    const int screen_height = 768;

    InitWindow(screen_width, screen_height, "Aeterium v0.1");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground({100, 149, 237, 255}); // Cornflower blue
        DrawText("Aeterium v0.1", 10, 10, 20, WHITE);
        DrawFPS(screen_width - 100, 10);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
