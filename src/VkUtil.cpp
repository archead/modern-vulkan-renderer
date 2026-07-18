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
	allocBuff.buffer = nullptr;
	allocBuff.allocation = nullptr;
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


void vkutil::generateMipmaps(vk::raii::PhysicalDevice& physicalDevice, vk::raii::Device& device, vk::raii::CommandPool& commandPool, vk::raii::Queue& graphicsQueue, vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {

	// Check if image format supports linear blit-ing
	vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(imageFormat);

	if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
		throw std::runtime_error("Texture image format does not support linear filtering");
	}

	vk::raii::CommandBuffer commandBuffer = vkutil::beginSingleTimeCommands(device, commandPool);

	vk::ImageMemoryBarrier barrier(
		vk::AccessFlagBits::eTransferWrite,
		vk::AccessFlagBits::eTransferRead,
		vk::ImageLayout::eTransferDstOptimal,
		vk::ImageLayout::eTransferSrcOptimal,
		vk::QueueFamilyIgnored,
		vk::QueueFamilyIgnored,
		image
		);

	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
		barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);

		vk::ArrayWrapper1D<vk::Offset3D,2> offsets, dstOffsets;
		offsets[0] = vk::Offset3D(0, 0, 0);
		offsets[1] = vk::Offset3D(mipWidth, mipHeight, 1);
		dstOffsets[0] = vk::Offset3D(0, 0, 0);
		dstOffsets[1] = vk::Offset3D(mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1);

		vk::ImageBlit blit{};
		blit.srcSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i-1, 0, 1);
		blit.srcOffsets = offsets;
		blit.dstSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1);
		blit.dstOffsets = dstOffsets;

		commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, {blit}, vk::Filter::eLinear);

		barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, barrier);
	vkutil::endSingleTimeCommands(commandBuffer, graphicsQueue);
}

void vkutil::transitionImageLayout(vk::raii::Device& device, vk::raii::CommandPool& commandPool, vk::raii::Queue& graphicsQueue, const vk::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels) {
	auto commandBuffer = vkutil::beginSingleTimeCommands(device, commandPool);
	vk::PipelineStageFlags sourceStage, destinationStage;

	vk::ImageMemoryBarrier barrier = {};
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.image = image;
	barrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1};

	if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
		barrier.srcAccessMask = {};
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

		sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
		destinationStage = vk::PipelineStageFlagBits::eTransfer;
	} else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

		sourceStage = vk::PipelineStageFlagBits::eTransfer;
		destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
	}   else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
	vkutil::endSingleTimeCommands(commandBuffer, graphicsQueue);
}

void vkutil::copyBufferToImage(vk::raii::Device& device, vk::raii::CommandPool& commandPool, vk::raii::Queue& graphicsQueue, const vk::Buffer& buffer, vk::Image image, vk::ImageLayout layout, const std::vector<vk::BufferImageCopy>& regions) {
	auto commandBuffer = vkutil::beginSingleTimeCommands(device, commandPool);
	commandBuffer.copyBufferToImage(buffer, image, layout, regions);
	vkutil::endSingleTimeCommands(commandBuffer, graphicsQueue);
}


[[nodiscard]] vk::raii::ShaderModule vkutil::createShaderModule(vk::raii::Device& device, const std::vector<char>& code) {
	vk::ShaderModuleCreateInfo createInfo;
	createInfo.codeSize = code.size() * sizeof(char);
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	vk::raii::ShaderModule shaderModule(device, createInfo);
	return shaderModule;
}

