#include "core/Engine.h"
#include <cstdio>
#include <exception>
#include <string>

// Usage: juliana [game_path]
//   game_path — directory containing a game definition.toml
//               defaults to packages/game
int main(int argc, char* argv[]) {
    std::string game_path = "packages/game";
    if (argc >= 2) game_path = argv[1];

    try {
        Engine engine;
        engine.Init(game_path);
        engine.Run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return 1;
    }
    return 0;
}
