cmake_minimum_required(VERSION 3.16)

project(CPU_Scheduling_Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(
    CPU_Scheduling_Simulator
    src/CPU_Scheduling_Simulator.cpp
)

if(MSVC)
    target_compile_options(CPU_Scheduling_Simulator PRIVATE /W4)
else()
    target_compile_options(CPU_Scheduling_Simulator PRIVATE -Wall -Wextra -pedantic)
endif()
