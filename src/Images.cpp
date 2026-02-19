#pragma once
#include "Renderer.hpp"
#include "Config.hpp"

void Renderer::createImage(
uint32_t width,
uint32_t height,
uint32_t mipLevels,
VkSampleCountFlagBits numSamples,
VkFormat format,
VkImageTiling tiling,
VkImageUsageFlags usage,
AllocatedImage& image) {

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = VkExtent3D{width, height, 1};
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = numSamples;
    imageInfo.tiling = tiling;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VkResult  res = vmaCreateImage(allocator, &imageInfo, &allocInfo, &image.image, &image.allocation, nullptr);
    if (res != VK_SUCCESS) {throw std::runtime_error("vmaCreateImage failed");}
}

void Renderer::destroyImage(VmaAllocator allocator, AllocatedImage& allocImage) {
    vmaDestroyImage(allocator, allocImage.image, allocImage.allocation);
}