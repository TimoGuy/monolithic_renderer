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

# Dependencies.
if(WIN32)
    # Windows renderer uses Vulkan and GLFW for windowing.
    find_package(Vulkan REQUIRED)
    add_subdirectory("third_party/glfw")
else()
    message(FATAL_ERROR "Unsupported OS.")
endif()

# Static library build.
add_library(${PROJECT_NAME} STATIC)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/example
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap
        ${Vulkan_INCLUDE_DIR}
)

target_sources(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include/monolithic_renderer_public.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/example/example_main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap/VkBootstrap.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap/VkBootstrap.h
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap/VkBootstrapDispatch.h
)

target_link_libraries(${PROJECT_NAME}
    glfw
    ${GLFW_LIBRARIES}
    Vulkan::Vulkan
)
