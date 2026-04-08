#include "core/GameLoop.h"
#include "core/Engine.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

GameLoop::GameLoop(int tick_rate)
    : tick_rate_(tick_rate)
    , tick_duration_(1.0 / tick_rate)
{
}

// ─── Emscripten ───────────────────────────────────────────────────────────────

#ifdef __EMSCRIPTEN__

struct EmLoopState {
    Engine*   engine;
    GameLoop* loop;
    uint64_t  last_time;
    double    accumulator;
};

static void em_tick(void* arg) {
    auto* s = static_cast<EmLoopState*>(arg);

    if (s->engine->ShouldQuit()) {
        emscripten_cancel_main_loop();
        return;
    }

    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t now  = SDL_GetPerformanceCounter();
    double frame_time = static_cast<double>(now - s->last_time) / freq;
    s->last_time = now;

    // Clamp to avoid spiral of death
    frame_time = std::min(frame_time, 0.25);
    s->accumulator += frame_time;

    while (s->accumulator >= s->loop->GetTickDuration()) {
        s->engine->SimTick(s->loop->GetTickDuration());
        s->accumulator -= s->loop->GetTickDuration();
    }

    double alpha = s->accumulator / s->loop->GetTickDuration();
    s->engine->Render(alpha);
}

#endif // __EMSCRIPTEN__

// ─── Run ──────────────────────────────────────────────────────────────────────

void GameLoop::Run(Engine& engine) {
#ifdef __EMSCRIPTEN__
    // State is static so it outlives the longjmp that simulate_infinite_loop=1
    // uses to unwind the C stack after scheduling the browser callback.
    static EmLoopState state;
    state.engine      = &engine;
    state.loop        = this;
    state.last_time   = SDL_GetPerformanceCounter();
    state.accumulator = 0.0;

    // fps=0 → run as fast as requestAnimationFrame allows (vsync-limited).
    // simulate_infinite_loop=1 → emulate a blocking call via longjmp so that
    // code after engine.Run() in main() is never reached.
    emscripten_set_main_loop_arg(em_tick, &state, 0, 1);
#else
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
#endif
}
