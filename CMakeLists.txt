cmake_minimum_required(VERSION 3.14)
project(juliana CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/lib/cmake/SDL2" "/opt/homebrew/lib/cmake/SDL2_ttf")
# Lua 5.4 via Homebrew (brew install lua@5.4)
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/lua@5.4" "/opt/homebrew/opt/lua")
# Windows/MSYS2 ucrt64
list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")

# --- System dependencies ---
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

# Lua 5.4 (required for scripting)
find_package(Lua 5.4)
if(NOT Lua_FOUND)
    find_package(Lua REQUIRED)
endif()

# --- Header-only dependencies via FetchContent ---
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

FetchContent_MakeAvailable(tomlplusplus json sol2)

# --- Engine executable ---
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    /opt/homebrew/include
    /usr/local/include
    "C:/msys64/ucrt64/include"
    ${LUA_INCLUDE_DIR}
    ${sol2_SOURCE_DIR}/include
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
    ${LUA_LIBRARIES}
)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
    )
endif()

# --- Symlink packages/ into build dir ---
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/packages
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/packages
)
