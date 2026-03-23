cmake_minimum_required(VERSION 3.14)
project(aeterium CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/lib/cmake/SDL2" "/opt/homebrew/lib/cmake/SDL2_ttf")

# --- System dependencies ---
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

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

FetchContent_MakeAvailable(tomlplusplus json)

# --- Engine executable ---
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src /opt/homebrew/include /usr/local/include)
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2::SDL2
    SDL2_ttf::SDL2_ttf
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
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
