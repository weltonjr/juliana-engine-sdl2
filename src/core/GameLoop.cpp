#include "core/GameLoop.h"
#include "core/Engine.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>

GameLoop::GameLoop(int tick_rate)
    : tick_rate_(tick_rate)
    , tick_duration_(1.0 / tick_rate)
{
}

void GameLoop::Run(Engine& engine) {
    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t last_time = SDL_GetPerformanceCounter();
    double accumulator = 0.0;

    int frame_count = 0;
    double fps_timer = 0.0;

    while (!engine.ShouldQuit()) {
        uint64_t now = SDL_GetPerformanceCounter();
        double frame_time = static_cast<double>(now - last_time) / freq;
        last_time = now;

        // Clamp to avoid spiral of death
        frame_time = std::min(frame_time, 0.25);
        accumulator += frame_time;

        // FPS counter
        fps_timer += frame_time;
        frame_count++;
        if (fps_timer >= 1.0) {
            // TODO: show FPS on top right of screen
            frame_count = 0;
            fps_timer -= 1.0;
        }

        while (accumulator >= tick_duration_) {
            engine.SimTick(tick_duration_);
            accumulator -= tick_duration_;
        }

        double alpha = accumulator / tick_duration_;
        engine.Render(alpha);
    }
}
