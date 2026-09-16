cmake_minimum_required(VERSION 3.16)
project(CPU_Scheduling_Simulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(CPU_Scheduling_Simulator src/CPU_Scheduling_Simulator.cpp)

if(WIN32)
    target_link_libraries(CPU_Scheduling_Simulator PRIVATE user32 gdi32)
endif()
if(MSVC)
    target_compile_options(CPU_Scheduling_Simulator PRIVATE /W4 /utf-8)
    set_property(TARGET CPU_Scheduling_Simulator PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    target_compile_options(CPU_Scheduling_Simulator PRIVATE -Wall -Wextra -pedantic)
    if(MINGW)
        target_link_options(CPU_Scheduling_Simulator PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
endif()

