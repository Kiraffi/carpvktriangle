cmake_minimum_required(VERSION 3.21)
project(carpvktriangle)

set(CMAKE_CXX_STANDARD 20)



add_executable(carpvktriangle src/main.cpp)

set(SDL_STATIC ON)
set(SDL_SHARED OFF)


add_subdirectory("external/carpvk")
add_subdirectory("external/SDL")



target_link_libraries(carpvktriangle PRIVATE
    carpvk
    SDL3::SDL3
)