cmake_minimum_required(VERSION 4.0)
project(vulkan_doc_tutorial)

set(CMAKE_CXX_STANDARD 20)

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
add_subdirectory(thirdparty/glfw)
add_subdirectory(thirdparty/glm)
add_subdirectory(thirdparty/tinyobjloader)

add_library(stb INTERFACE)
target_include_directories(stb
    INTERFACE
        thirdparty/stb)

add_executable(vulkan_doc_tutorial main.cpp)

set (SHADER_SLANG_SOURCES ${CMAKE_CURRENT_LIST_DIR}/shaders/shader.slang)
add_slang_shader_target(shaders SOURCES ${SHADER_SLANG_SOURCES})
add_dependencies(vulkan_doc_tutorial shaders)

target_link_libraries(vulkan_doc_tutorial
        PRIVATE
        glfw
        Vulkan::Vulkan
        glm::glm
        tinyobjloader
        stb
)