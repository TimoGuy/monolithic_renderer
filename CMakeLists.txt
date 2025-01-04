cmake_minimum_required(VERSION 3.28)

project(monolithic_renderer
    VERSION 0.1.0
    LANGUAGES CXX
)

# C/C++ standard.
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Multithreaded build.
set(CMAKE_BUILD_PARALLEL_LEVEL 16)
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

# Static library build for `monolithic_renderer_component`
add_library(monolithic_renderer_component STATIC)

target_include_directories(monolithic_renderer_component
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        # ${CMAKE_CURRENT_SOURCE_DIR}/src/example
)

target_sources(monolithic_renderer_component
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/monolithic_renderer_public.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer.h
)
