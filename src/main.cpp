#include "core/Engine.h"
#include <cstdio>
#include <exception>

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        Engine engine;
        engine.Init();
        engine.Run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return 1;
    }
    return 0;
}
