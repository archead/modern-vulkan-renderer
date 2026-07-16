#include "VkUtil.hpp"

void vkutil::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,vk::raii::Device& device, vk::raii::CommandPool& commandPool, vk::raii::Queue& queue) {
    vk::raii::CommandBuffer cmd = beginSingleTimeCommands(device, commandPool);
    cmd.copyBuffer(vk::Buffer(srcBuffer), vk::Buffer(dstBuffer), vk::BufferCopy(0, 0, size));
    endSingleTimeCommands(cmd, queue);
}

void vkutil::createBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, AllocatedBuffer &allocBuff, bool hostVisible) {

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    if (hostVisible) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    const VkResult res = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &allocBuff.buffer, &allocBuff.allocation, &allocBuff.allocInfo);

    if (res != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateBuffer failed!");
    }
}

void vkutil::destroyBuffer(VmaAllocator allocator, AllocatedBuffer& allocBuff) {
    vmaDestroyBuffer(allocator, allocBuff.buffer, allocBuff.allocation);
}

vk::raii::CommandBuffer vkutil::beginSingleTimeCommands(vk::raii::Device& device, vk::raii::CommandPool& commandPool) {
    vk::CommandBufferAllocateInfo allocInfo = {};
    allocInfo.commandPool = commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());

    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void vkutil::endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer, vk::raii::Queue& graphicsQueue) {
    commandBuffer.end();

    vk::SubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*commandBuffer;

    graphicsQueue.submit(submitInfo);
    graphicsQueue.waitIdle();
}

vk::Format vkutil::findSupportedFormat(vk::raii::PhysicalDevice& physicalDevice, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for (const auto format : candidates) {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

vk::Format vkutil::findDepthFormat(vk::raii::PhysicalDevice& physicalDevice) {
    return findSupportedFormat(
    physicalDevice,
      {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
      vk::ImageTiling::eOptimal,
      vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

void vkutil::createImage(
VmaAllocator allocator,
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

void vkutil::destroyImage(VmaAllocator allocator, AllocatedImage& allocImage) {
    vmaDestroyImage(allocator, allocImage.image, allocImage.allocation);
}

vk::raii::ImageView vkutil::createImageView(vk::raii::Device& device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) {
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.image = image;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.format = format;
    viewInfo.subresourceRange = {aspectFlags, 0, mipLevels, 0, 1};

    return vk::raii::ImageView(device, viewInfo);
}
