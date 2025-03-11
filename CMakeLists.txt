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
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/camera.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fastgltf_support__cglm_element_traits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/geo_instance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/geo_instance.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/gltf_loader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_system.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_system.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/material_bank.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/material_bank.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/renderer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer_win64_vk_buffer__allocated_buffer.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv_reflect/spirv_reflect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv_reflect/spirv_reflect.h
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
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spirv_reflect
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vk-bootstrap
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/VulkanMemoryAllocator/include
        ${cglm_INCLUDE_DIR}
        ${input_handling_INCLUDE_DIR}
        ${multithreaded_job_system_INCLUDE_DIR}
        ${Vulkan_INCLUDE_DIR}
        ${timing_reporter_macro_magic_INCLUDE_DIR}
        ${world_entity_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
    fastgltf
    glfw
    ${GLFW_LIBRARIES}
    Vulkan::Vulkan
    multithreaded_job_system
)

# Compile shaders.
set(SHADER_SRC_DIR
    ${CMAKE_CURRENT_SOURCE_DIR}/shader_src)

set(shader_includes
    ${SHADER_SRC_DIR}/geom_static_mesh_vert.glsl
    ${SHADER_SRC_DIR}/geom_instance_data_br.glsl
    ${SHADER_SRC_DIR}/geom_camera_set0.glsl
    ${SHADER_SRC_DIR}/geom_material_sets_set1.glsl
    ${SHADER_SRC_DIR}/geom_vert_helper_functions.glsl
    ${SHADER_SRC_DIR}/geom_material_sets_helper_functions.glsl
)

set(all_shaders
    ${SHADER_SRC_DIR}/colored_triangle.frag
    ${SHADER_SRC_DIR}/colored_triangle.vert
    ${SHADER_SRC_DIR}/geom_culling.comp
    ${SHADER_SRC_DIR}/geom_write_draw_cmds.comp
    ${SHADER_SRC_DIR}/geommat_missing.frag
    ${SHADER_SRC_DIR}/geommat_missing.vert
    ${SHADER_SRC_DIR}/geommat_opaque_z_prepass.frag
    ${SHADER_SRC_DIR}/geommat_opaque_z_prepass.vert
    ${SHADER_SRC_DIR}/geommat_opaque_shadow.frag
    ${SHADER_SRC_DIR}/geommat_opaque_shadow.vert
    ${SHADER_SRC_DIR}/gradient.comp
)

if(WIN32)
    # Use glslc.
    list(LENGTH all_shaders FILE_COUNT)
    if(FILE_COUNT EQUAL 0)
        message(FATAL_ERROR "Cannot create a shaders target without any source files.")
    endif()

    set(ALL_SHADER_PRODUCTS)
        
    foreach(SHADER_SOURCE IN LISTS all_shaders)
        cmake_path(ABSOLUTE_PATH SHADER_SOURCE NORMALIZE)
        cmake_path(GET SHADER_SOURCE FILENAME SHADER_NAME)

        set(SHADER_PRODUCT "${ASSET_DIR}/shaders/${SHADER_NAME}.spv")
        list(APPEND ALL_SHADER_PRODUCTS "${SHADER_PRODUCT}")
        message("Found shader: \"" ${SHADER_NAME} "\"")

        # Build command.
        set(SHADER_COMMAND)
        list(APPEND SHADER_COMMAND Vulkan::glslc)
        list(APPEND SHADER_COMMAND "${SHADER_SOURCE}")
        list(APPEND SHADER_COMMAND "--target-env=vulkan1.3")
        list(APPEND SHADER_COMMAND "-Werror")  # Treat warnings as errors.
        # @NOTE: have driver optimize shaders. Apparently this can be harmful for desktop gpus.
        # list(APPEND SHADER_COMMAND "-O")       # Optimize for performance.
        list(APPEND SHADER_COMMAND "-o")
        list(APPEND SHADER_COMMAND "${SHADER_PRODUCT}")

        add_custom_command(
            OUTPUT ${SHADER_PRODUCT}
            COMMAND ${SHADER_COMMAND}
            COMMENT "Compiling shader: \"${SHADER_NAME}\""
            DEPENDS ${SHADER_SOURCE} ${shader_includes}
        )
    endforeach()

    add_custom_target(${PROJECT_NAME}_shaders ALL
        DEPENDS ${ALL_SHADER_PRODUCTS}
    )
else()
    message(FATAL_ERROR "Unsupported Shader Compiler.")
endif()
