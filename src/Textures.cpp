#include "Renderer.hpp"
#include <ktx.h>
#include <ktxvulkan.h>
#include "VkUtil.hpp"

void Renderer::createTextureImage() {
    // Load KTX texture instead of using std_image
    ktxTexture2* kTexture;
    KTX_error_code result = ktxTexture2_CreateFromNamedFile(TEXTURE_PATH, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);

    if (result != KTX_SUCCESS) { throw std::runtime_error("failed to load ktx texture image!"); }
    if (kTexture->vkFormat == VK_FORMAT_UNDEFINED) { ktxTexture2_Destroy(kTexture); throw std::runtime_error("KTX2 has VK_FORMAT UNDEFINED (needs transcoding)"); }

    mipLevels = kTexture->numLevels;
    textureFormat =	static_cast<vk::Format>(kTexture->vkFormat);

    ktxVulkanDeviceInfo deviceInfo{};
    result = ktxVulkanDeviceInfo_Construct(&deviceInfo, *physicalDevice, *device, *graphicsQueue, *commandPool, nullptr);

    if (result != KTX_SUCCESS) { throw std::runtime_error("ktxVulkanDeviceInfo_Construct() failed!"); }

    // does all the hard work, creates staging buffer, a VkImage buffer, does all the transitions, all is left is to use the created ktxVulkanTexture object
    ktxVulkanTexture m_ktxVkTexture{};
    result = ktxTexture2_VkUploadEx(kTexture, &deviceInfo, &m_ktxVkTexture, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    ktxVkTexture = m_ktxVkTexture;

    if (result != KTX_SUCCESS) { ktxVulkanDeviceInfo_Destruct(&deviceInfo); ktxTexture2_Destroy(kTexture); throw std::runtime_error("ktxTexture_VkUploadEx() failed!"); }

    textureImage.image = m_ktxVkTexture.image;
    textureImage.memory = m_ktxVkTexture.deviceMemory;

    ktxVulkanDeviceInfo_Destruct(&deviceInfo);
    ktxTexture2_Destroy(kTexture);
}

void Renderer::createTextureSampler() {
    vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();

    vk::SamplerCreateInfo samplerInfo   = {};
    samplerInfo.magFilter               = vk::Filter::eLinear;
    samplerInfo.minFilter               = vk::Filter::eLinear;
    samplerInfo.mipmapMode              = vk::SamplerMipmapMode::eLinear;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = vk::LodClampNone;
    samplerInfo.addressModeU            = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV            = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW            = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable        = vk::True;
    samplerInfo.maxAnisotropy           = properties.limits.maxSamplerAnisotropy;
    samplerInfo.compareEnable           = vk::False;
    samplerInfo.compareOp               = vk::CompareOp::eAlways;
    samplerInfo.borderColor             = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = vk::False;

    textureSampler = vk::raii::Sampler(device, samplerInfo);
}

std::unique_ptr<Renderer::ModelTexture> Renderer::loadTextureKTX(const char* texturePath) {

    auto mTex = std::make_unique<ModelTexture>();
    mTex->device = *device;
    mTex->sampler = *textureSampler;

    // Load KTX texture instead of using std_image
    ktxTexture2* kTexture;
    KTX_error_code result = ktxTexture2_CreateFromNamedFile(texturePath, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &kTexture);

    if (result != KTX_SUCCESS) { throw std::runtime_error("failed to load ktx texture image!"); } if (kTexture->vkFormat == VK_FORMAT_UNDEFINED) { ktxTexture2_Destroy(kTexture); throw std::runtime_error("KTX2 has VK_FORMAT UNDEFINED (needs transcoding)"); }
    mipLevels = kTexture->numLevels;
    textureFormat =	static_cast<vk::Format>(kTexture->vkFormat);

    ktxVulkanDeviceInfo deviceInfo{};
    result = ktxVulkanDeviceInfo_Construct(&deviceInfo, *physicalDevice, *device, *graphicsQueue, *commandPool, nullptr);

    if (result != KTX_SUCCESS) { throw std::runtime_error("ktxVulkanDeviceInfo_Construct() failed!"); }

    // does all the hard work, creates staging buffer, a VkImage buffer, does all the transitions, all is left is to use the created ktxVulkanTexture object
    result = ktxTexture2_VkUploadEx(kTexture, &deviceInfo, &mTex->ktxVkTexture, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (result != KTX_SUCCESS) { ktxVulkanDeviceInfo_Destruct(&deviceInfo); ktxTexture2_Destroy(kTexture); throw std::runtime_error("ktxTexture_VkUploadEx() failed!"); }
	//TODO ensure that normals are loaded as VK_FORMAT_R8G8B8A8_UNORM instead of SRGB to prevent unnecessary gamma correction
    mTex->imageView = vkutil::createImageView(device, mTex->ktxVkTexture.image, static_cast<vk::Format>(kTexture->vkFormat), vk::ImageAspectFlagBits::eColor, mTex->ktxVkTexture.levelCount);

    ktxVulkanDeviceInfo_Destruct(&deviceInfo);
    ktxTexture2_Destroy(kTexture);


    return mTex;
}

void Renderer::generateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {

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
