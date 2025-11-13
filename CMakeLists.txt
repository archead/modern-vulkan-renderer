cmake_minimum_required(VERSION 4.0)
project(vulkan_doc_tutorial)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
add_subdirectory(thirdparty/glfw)
add_subdirectory(thirdparty/glm)
add_subdirectory(thirdparty/tinyobjloader)

add_library(stb INTERFACE)
target_include_directories(stb
    INTERFACE
        thirdparty/stb)

add_executable(vulkan_doc_tutorial main.cpp)

target_link_libraries(vulkan_doc_tutorial
        PRIVATE
        glfw
        Vulkan::Vulkan
        glm::glm
        tinyobjloader
        stb
)