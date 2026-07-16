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

	void generateMipmaps(vk::raii::PhysicalDevice& physicalDevice, vk::raii::Device& device, vk::raii::CommandPool& commandPool, vk::raii::Queue& graphicsQueue, vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	void transitionImageLayout(vk::raii::Device& device, vk::raii::CommandPool& commandPool, vk::raii::Queue& graphicsQueue, const vk::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels);

	void copyBufferToImage(vk::raii::Device& device, vk::raii::CommandPool& commandPool, vk::raii::Queue& graphicsQueue, const vk::Buffer& buffer, vk::Image image, vk::ImageLayout layout, const std::vector<vk::BufferImageCopy>& regions);

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(vk::raii::Device& device, const std::vector<char>& code);
}
