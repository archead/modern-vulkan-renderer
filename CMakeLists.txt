cmake_minimum_required(VERSION 4.0)
project(vulkan_doc_tutorial)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR ${CMAKE_CURRENT_LIST_DIR}/thirdparty/imgui)

function (add_slang_shader_target TARGET)
    cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
    set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
    set (ENTRY_POINTS -entry vertMain -entry fragMain)
    set (SLANGC_EXECUTABLE "C:/VulkanSDK/1.4.321.1/bin/slangc.exe")
    add_custom_command (
            OUTPUT ${SHADERS_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
    )
    add_custom_command (
            OUTPUT  ${SHADERS_DIR}/slang.spv
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES}
                -target spirv
                -profile spirv_1_4
                -emit-spirv-directly
                -fvk-use-entrypoint-name ${ENTRY_POINTS}
                -o slang.spv
            WORKING_DIRECTORY ${SHADERS_DIR}
            DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
            COMMENT "Compiling Slang Shaders"
            VERBATIM
    )
    add_custom_target (${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
endfunction()

find_package(Vulkan REQUIRED)
add_subdirectory(thirdparty/glm)
add_subdirectory(thirdparty/tinyobjloader)
add_subdirectory(thirdparty/vk-bootstrap)
add_subdirectory(thirdparty/VulkanMemoryAllocator)
add_subdirectory(thirdparty/SDL)
add_subdirectory(thirdparty/tinygltf)
add_subdirectory(thirdparty/ktx)


add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)
target_link_libraries(imgui PUBLIC
    Vulkan::Vulkan
    SDL3::SDL3
)

add_library(stb INTERFACE)
target_include_directories(stb
    INTERFACE
        thirdparty/stb
)

add_library(mikktspace STATIC
    thirdparty/MikkTSpace/mikktspace.c
)
target_include_directories(mikktspace
    PUBLIC
        thirdparty/MikkTSpace
)

add_executable(vulkan_doc_tutorial
    main.cpp
    src/Renderer.cpp
    src/Buffers.cpp
    src/DescriptorSets.cpp
    src/Allocator.cpp
    src/CommandBuffers.cpp
    src/Images.cpp
    src/Textures.cpp
)

target_include_directories(vulkan_doc_tutorial
    PRIVATE
        include
)

set (SHADER_SLANG_SOURCES ${CMAKE_CURRENT_LIST_DIR}/shaders/shader.slang)
add_slang_shader_target(shaders SOURCES ${SHADER_SLANG_SOURCES})
add_dependencies(vulkan_doc_tutorial shaders)

target_link_libraries(vulkan_doc_tutorial
        PRIVATE
        Vulkan::Vulkan
        glm::glm
        tinyobjloader
        stb
        vk-bootstrap::vk-bootstrap
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
        tinygltf
        ktx
        imgui
        mikktspace
)