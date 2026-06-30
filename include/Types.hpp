#pragma once
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#endif

#include <glm/glm.hpp>
#include "Config.hpp"
#include <vk_mem_alloc.h>

struct AllocatedBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo{}; // optional;
};

struct AllocatedImage {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE; // mainly used with KTX2 due to UploadEX()
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

struct VertexHasher {
    size_t operator()(Vertex const& v) const noexcept {
        size_t seed = 0;

        auto combine = [&](size_t h) {
            seed ^= h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        };

        combine(std::hash<float>{}(v.pos.x));
        combine(std::hash<float>{}(v.pos.y));
        combine(std::hash<float>{}(v.pos.z));

        combine(std::hash<float>{}(v.color.x));
        combine(std::hash<float>{}(v.color.y));
        combine(std::hash<float>{}(v.color.z));

        combine(std::hash<float>{}(v.texCoord.x));
        combine(std::hash<float>{}(v.texCoord.y));

        return seed;
    }
};

struct TransformUBO {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 normalMatrix;
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
