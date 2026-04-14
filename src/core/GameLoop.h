#pragma once

#include <cstdint>

class Engine;

class GameLoop {
public:
    GameLoop(int tick_rate = 60);

    void Run(Engine& engine);

    int GetTickRate() const { return tick_rate_; }
    double GetTickDuration() const { return tick_duration_; }
    int GetActualFPS() const { return actual_fps_; }

private:
    int tick_rate_;
    double tick_duration_;
    int actual_fps_ = 0;
};
