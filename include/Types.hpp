#pragma once
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#endif

#include <glm/glm.hpp>
#include "Config.hpp"
#include <vk_mem_alloc.h>

#include "ktxvulkan.h"

struct VulkanContext {
    vk::raii::Device&         device;
    vk::raii::PhysicalDevice& physicalDevice;
    vk::raii::CommandPool&    commandPool;
    vk::raii::Queue&          graphicsQueue;
    VmaAllocator              allocator = nullptr;
};

struct AllocatedBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo{}; // optional;
};

struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo{}; // optional
};

struct AllocatedUniformBuffer {
    AllocatedBuffer buffer;
    void* mapped = nullptr;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    glm::vec3 normal;
    glm::vec4 tangent; // tangent vec3 + handedness

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 5> getAttributeDescriptions() {
        return{
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord)),
            vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)),
            vk::VertexInputAttributeDescription(4, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Vertex, tangent))
        };
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal && tangent == other.tangent;
    }
};

 struct GlobalUBO {
    glm::mat4 view;
    glm::mat4 proj;
};

struct ObjectUBO {
    glm::mat4 model;
    glm::mat4 normalMatrix;
    glm::uvec4 flags = {0, 0, 0, 0}; // x = use normal map
};

enum class DebugState : int32_t {Lit, Albedo, Normal, Position};

struct MikkTSpaceUserData {
    std::vector<Vertex>* vertices;
    std::vector<uint32_t>* indices;
};

struct Primitive {
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t vertexOffset;
    int32_t materialIndex;
};

struct Mesh { std::vector<Primitive> primitives; };

struct Material {
    int baseColorTex = -1;
    int normalTex = -1;
    int metalRoughTex = -1;
    int occlusionTex = -1;
    int emissiveTex = -1;

    glm::vec4 baseColorFactor{1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    bool doubleSided = false;
};

struct ModelTexture {
    ktxVulkanTexture             ktxVkTexture = {};
    const VkAllocationCallbacks *allocator    = nullptr;
    VkDevice                     device       = VK_NULL_HANDLE;
    vk::raii::ImageView          imageView    = nullptr;

    ~ModelTexture() {
        if (device && ktxVkTexture.image != VK_NULL_HANDLE) {
            ktxVulkanTexture_Destruct(&ktxVkTexture, device, allocator);
            ktxVkTexture = {};
        }
    }

    // remove copy and assign operators / constructors
    ModelTexture() = default;
    ModelTexture(const ModelTexture&) = delete;
    ModelTexture& operator=(const ModelTexture&) = delete;
};

struct DrawItem {
    int32_t meshIndex;
    glm::mat4 transformMatrix;
};

