cmake_minimum_required(VERSION 3.20)
project(cell-battles)

set(CMAKE_CXX_STANDARD 17)

find_package(SFML 2 REQUIRED COMPONENTS graphics system window)

file(GLOB SOURCES src/*.cpp src/world/*.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC "include" "include/cell-battles")
target_link_libraries(${PROJECT_NAME} sfml-graphics sfml-window sfml-system)