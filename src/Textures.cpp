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

