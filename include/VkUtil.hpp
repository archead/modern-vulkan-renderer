#pragma once
#include <vulkan/vulkan_raii.hpp>
#include "Types.hpp"
namespace vkutil {

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,vk::raii::Device& device, vk::raii::CommandPool& commandPool, vk::raii::Queue& queue);

    void createBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, AllocatedBuffer &allocBuff, bool hostVisible);

    void destroyBuffer(VmaAllocator allocator, AllocatedBuffer& allocBuff);

    vk::raii::CommandBuffer beginSingleTimeCommands(vk::raii::Device& device, vk::raii::CommandPool& commandPool);

    void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer, vk::raii::Queue& graphicsQueue);
}
