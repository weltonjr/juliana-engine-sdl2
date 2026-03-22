#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "Game.h"
#include "core/Types.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    const int screen_w = 1024;
    const int screen_h = 768;

    SDL_Window* window = SDL_CreateWindow(
        "Aeterium v0.1",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        screen_w, screen_h,
        SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    Game game(screen_w, screen_h, renderer);

    Uint64 prev        = SDL_GetPerformanceCounter();
    float  accumulator = 0.0f;

    while (!game.quit_requested()) {
        Uint64 now = SDL_GetPerformanceCounter();
        float  dt  = (float)(now - prev) / (float)SDL_GetPerformanceFrequency();
        prev = now;
        if (dt > 0.05f) dt = 0.05f; // cap at 50 ms

        accumulator += dt;

        game.poll_events(); // drain SDL event queue once per frame

        while (accumulator >= PHYSICS_DT) {
            game.update(PHYSICS_DT);
            accumulator -= PHYSICS_DT;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        game.draw(renderer);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
