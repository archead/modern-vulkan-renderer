#pragma once
#include <vulkan/vulkan_raii.hpp>
#include "Types.hpp"
namespace vkutil {

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,vk::raii::Device& device, vk::raii::CommandPool& commandPool, vk::raii::Queue& queue);

    void createBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, AllocatedBuffer &allocBuff, bool hostVisible);

    void destroyBuffer(VmaAllocator allocator, AllocatedBuffer& allocBuff);

    vk::raii::CommandBuffer beginSingleTimeCommands(vk::raii::Device& device, vk::raii::CommandPool& commandPool);

    void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer, vk::raii::Queue& graphicsQueue);

    vk::Format findSupportedFormat(vk::raii::PhysicalDevice& physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

    vk::Format findDepthFormat(vk::raii::PhysicalDevice& physicalDevice);

    void createImage(
    VmaAllocator          allocator,
    uint32_t              width,
    uint32_t              height,
    uint32_t              mipLevels,
    VkSampleCountFlagBits numSamples,
    VkFormat              format,
    VkImageTiling         tiling,
    VkImageUsageFlags     usage,
    AllocatedImage &      image);

    void destroyImage(VmaAllocator allocator, AllocatedImage& allocImage);

	vk::raii::ImageView createImageView(vk::raii::Device& device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);
}
