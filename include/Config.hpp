#pragma once
#include <cstdint>

inline constexpr uint32_t WIDTH = 800;
inline constexpr uint32_t HEIGHT = 600;
inline constexpr const char* MODEL_PATH = R"(C:\dev\vulkan-doc-tutorial\models\sphere.gltf)";
inline constexpr const char* TEXTURE_PATH = "C:\\dev\\vulkan-doc-tutorial\\textures\\2d_rgb8.ktx2";
// Objects to render
inline constexpr int MAX_OBJECTS = 4;



inline const std::array<const char*, 1> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif