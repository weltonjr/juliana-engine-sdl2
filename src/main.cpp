#include "core/Engine.h"
#include <cstdio>
#include <exception>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Usage: juliana [game_path]
//   game_path — directory containing a game definition.toml
//               defaults to packages/mapeditor/game
int main(int argc, char* argv[]) {
    std::string game_path = "packages/mapeditor/game";
    if (argc >= 2) game_path = argv[1];

#ifdef __EMSCRIPTEN__
    // Static storage so the Engine outlives the longjmp that simulate_infinite_loop
    // performs after emscripten_set_main_loop_arg unwinds the native call stack.
    static Engine engine;
    engine.Init(game_path);
    engine.Run();  // schedules em_tick via emscripten_set_main_loop_arg, then longjmps out
#else
    try {
        Engine engine;
        engine.Init(game_path);
        engine.Run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return 1;
    }
#endif
    return 0;
}
