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
    add_subdirectory(third_party/glfw)
else()
    message(FATAL_ERROR "Unsupported OS.")
endif()
add_subdirectory(third_party/fastgltf)

# Static library build.
add_library(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf_loader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_descriptor_layout_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_descriptor_layout_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_immediate_submit.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_immediate_submit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_pipeline_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_pipeline_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_util.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_util.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap/VkBootstrap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap/VkBootstrap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap/VkBootstrapDispatch.h

    # dear ImGui source files.
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_glfw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_vulkan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_vulkan.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imconfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_internal.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imstb_rectpack.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imstb_textedit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imstb_truetype.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fastgltf/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include
        ${cglm_INCLUDE_DIR}
        ${multithreaded_job_system_INCLUDE_DIR}
        ${Vulkan_INCLUDE_DIR}
        ${world_entity_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    fastgltf
    glfw
    ${GLFW_LIBRARIES}
    Vulkan::Vulkan
    multithreaded_job_system
)
