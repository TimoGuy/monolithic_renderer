cmake_minimum_required(VERSION 3.28)

# Project
if(DEFINED monolithic_renderer_GUARD)
    if(NOT monolithic_renderer_GUARD STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
        return() # Project has been already included by someone else
    endif()
else()
    set(monolithic_renderer_GUARD ${CMAKE_CURRENT_BINARY_DIR} CACHE INTERNAL "monolithic_renderer guard")
endif()
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

# Dependencies.
if(WIN32)
    # Windows renderer uses Vulkan and GLFW for windowing.
    find_package(Vulkan REQUIRED)
    add_subdirectory("third_party/glfw")
else()
    message(FATAL_ERROR "Unsupported OS.")
endif()

# Static library build.
add_library(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/renderer.h
    # ${CMAKE_CURRENT_SOURCE_DIR}/src/example/example_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap/VkBootstrap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap/VkBootstrap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap/VkBootstrapDispatch.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/example
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include
        ${cglm_INCLUDE_DIR}
        ${multithreaded_job_system_INCLUDE_DIR}
        ${Vulkan_INCLUDE_DIR}
        ${world_entity_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    glfw
    ${GLFW_LIBRARIES}
    Vulkan::Vulkan
    multithreaded_job_system
)
