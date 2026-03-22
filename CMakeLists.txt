cmake_minimum_required(VERSION 3.14)
project(aeterium CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/lib/cmake/SDL2" "/opt/homebrew/lib/cmake/SDL2_ttf")

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE src /opt/homebrew/include /usr/local/include)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2_ttf::SDL2_ttf)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
    )
endif()
