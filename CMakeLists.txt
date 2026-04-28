cmake_minimum_required(VERSION 3.14)
project(juliana C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── Header-only dependencies (native + web) ──────────────────────────────────
include(FetchContent)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)

FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

# sol2 — header-only C++ bindings for Lua (requires Lua to be installed)
FetchContent_Declare(sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG v3.3.0
)

# Box2D 2.4.1 — rigid body physics for entities and dynamic terrain fragments
FetchContent_Declare(box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG v2.4.1
)

# Disable Box2D tests and benchmarks to avoid assembly issues in doctest.h
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_TESTBED    OFF CACHE BOOL "" FORCE)

# RmlUi 6.2 — HTML/CSS-subset UI library with native Lua bindings.
# We vendor our own SDL2 backend under src/ui/backend/, so samples are disabled.
FetchContent_Declare(rmlui
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG 6.2
)

set(RMLUI_SAMPLES         OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING         OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS    ON  CACHE BOOL "" FORCE)
set(RMLUI_FONT_ENGINE     "freetype" CACHE STRING "" FORCE)

# Dear ImGui 1.92.7 — immediate-mode GUI used exclusively for engine debug
# overlays (FPS, log console, entity inspector). Game UI uses RmlUi.
# ImGui ships no CMakeLists, so we add its sources to the engine target below.
# Lua bindings are a small hand-rolled surface in src/scripting/LuaImGuiBindings.cpp
# (the public sol2_ImGui_Bindings library is unmaintained and stuck on ImGui ~1.85).
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.7
)

FetchContent_MakeAvailable(tomlplusplus json sol2 box2d rmlui imgui)

# ─── Patch sol2 v3.3.0 ────────────────────────────────────────────────────────
# optional<T&>::emplace incorrectly calls this->construct() which only exists
# in the non-reference optional base class. The bug triggers a hard compile
# error on Emscripten's Clang. Patch once at configure time via an external
# script to avoid CMake string-escape issues with \t and \n characters.
execute_process(
    COMMAND python3 "${CMAKE_SOURCE_DIR}/cmake/patch_sol2.py"
        "${sol2_SOURCE_DIR}/include/sol/optional_implementation.hpp"
    RESULT_VARIABLE _sol2_patch_result
)

# ─── Platform-dependent: SDL2, SDL2_ttf, Lua ──────────────────────────────────
if(EMSCRIPTEN)
    # SDL2 and SDL2_ttf are provided by Emscripten ports (no find_package needed).
    # Lua must be compiled from source since there is no system Lua in the Emscripten sysroot.
    FetchContent_Declare(lua_src
        GIT_REPOSITORY https://github.com/lua/lua.git
        GIT_TAG v5.4.6
    )
    FetchContent_MakeAvailable(lua_src)

    file(GLOB LUA_SOURCES "${lua_src_SOURCE_DIR}/*.c")
    # Remove the standalone interpreter and compiler mains.
    list(REMOVE_ITEM LUA_SOURCES "${lua_src_SOURCE_DIR}/lua.c")
    list(REMOVE_ITEM LUA_SOURCES "${lua_src_SOURCE_DIR}/luac.c")

    add_library(lua54_wasm STATIC ${LUA_SOURCES})
    target_include_directories(lua54_wasm PUBLIC "${lua_src_SOURCE_DIR}")

    set(LUA_LIBRARIES lua54_wasm)
    set(LUA_INCLUDE_DIR "${lua_src_SOURCE_DIR}")
else()
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/lib/cmake/SDL2" "/opt/homebrew/lib/cmake/SDL2_ttf" "/opt/homebrew/lib/cmake/SDL2_image")
    # Lua 5.4 via Homebrew (brew install lua@5.4)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/lua@5.4" "/opt/homebrew/opt/lua")
    # FreeType for RmlUi (brew install freetype on macOS, libfreetype-dev on Linux)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/freetype")
    # Windows/MSYS2 ucrt64
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")

    find_package(SDL2 REQUIRED)
    find_package(SDL2_ttf REQUIRED)
    find_package(SDL2_image REQUIRED)

    find_package(Lua 5.4)
    if(NOT Lua_FOUND)
        find_package(Lua REQUIRED)
    endif()
endif()

# ─── Engine executable ─────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Dear ImGui sources (core + SDL2 platform + SDL_Renderer backend).
# ImGui ships no CMake target; we compile its .cpp files directly into juliana.
set(IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${LUA_INCLUDE_DIR}
    ${sol2_SOURCE_DIR}/include
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Vendored RmlUi SDL backend files require this define to pick the SDL2 code path.
target_compile_definitions(${PROJECT_NAME} PRIVATE RMLUI_SDL_VERSION_MAJOR=2)

if(NOT EMSCRIPTEN)
    target_include_directories(${PROJECT_NAME} PRIVATE
        /opt/homebrew/include
        /usr/local/include
        "C:/msys64/ucrt64/include"
    )
endif()

# Header-only libs (always linked)
target_link_libraries(${PROJECT_NAME} PRIVATE
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
    ${LUA_LIBRARIES}
    box2d
    RmlUi::RmlUi
)

if(EMSCRIPTEN)
    # Output as a self-contained HTML page that includes the .wasm and .data files.
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")

    # -sUSE_SDL=2 / USE_SDL_TTF=2 must be present at both compile and link time.
    target_compile_options(${PROJECT_NAME} PRIVATE
        -sUSE_SDL=2
        -sUSE_SDL_TTF=2
        -sUSE_SDL_IMAGE=2
        -sUSE_FREETYPE=1
        -sSDL2_IMAGE_FORMATS='["png"]'
        # Enable C++ exceptions so sol2 and toml++ work correctly.
        -fexceptions
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        -sUSE_SDL=2
        -sUSE_SDL_TTF=2
        -sUSE_SDL_IMAGE=2
        -sUSE_FREETYPE=1
        -sSDL2_IMAGE_FORMATS='["png"]'
        -sALLOW_MEMORY_GROWTH=1
        -sWASM=1
        # Full POSIX filesystem API (needed for std::filesystem and Lua file I/O).
        -sFORCE_FILESYSTEM=1
        # Match the compile-time exception flag.
        -fexceptions
        -sDISABLE_EXCEPTION_CATCHING=0
        # Embed all game packages into the virtual filesystem at /packages/
        --preload-file "${CMAKE_SOURCE_DIR}/packages@/packages"
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL2::SDL2
        SDL2_ttf::SDL2_ttf
        SDL2_image::SDL2_image
    )

    if(APPLE)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "-framework Cocoa"
            "-framework IOKit"
        )
    endif()

    # Symlink packages/ into the build directory so the engine finds assets.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_SOURCE_DIR}/packages
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/packages
    )
endif()
