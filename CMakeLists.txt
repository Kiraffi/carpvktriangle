cmake_minimum_required(VERSION 3.27)
project(carpvktriangle)

set(CMAKE_CXX_STANDARD 17)



add_executable(carpvktriangle src/main.cpp)

add_subdirectory("external/carpvk")



target_link_libraries(carpvktriangle PRIVATE
    carpvk
)