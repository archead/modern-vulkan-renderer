#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#endif
#pragma once
#include <iostream>
#include <stdexcept>

#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <chrono>
#include <unordered_map> // Using for deduplicating OBJ vertices

#include <fstream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "Types.hpp"
#include "Renderer.hpp"
#include "Config.hpp"

static std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!");
	}

	std::vector<char> buffer(file.tellg());

	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

	file.close();
	return buffer;
}

void Renderer::run() {
	initVulkan();
	mainLoop();
	cleanup();
}

void Renderer::handleBootstrapErrors(auto obj_ret) {
	if (!obj_ret) {
		std::cerr << obj_ret.error().message() << "\n";
		for (auto& r : obj_ret.detailed_failure_reasons())
			std::cerr << "  - " << r << "\n";
		throw std::runtime_error("failed to select physical device!");
	}
}

void Renderer::bootstrapVulkan() {
	// ---- Create Window
	SDL_Init(SDL_INIT_VIDEO);
	window = SDL_CreateWindow("Vulkan", WIDTH, HEIGHT, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	// ---- Create Instance
	vkb::InstanceBuilder instance_builder;

	uint32_t sdlExtCount = 0;
	char const* const* sdlInstanceExtensions = {};
	sdlInstanceExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);

	auto instance_ret = instance_builder
		.set_app_name("Hello Triangle")
		.set_engine_name("No Engine")
		.require_api_version(1,4,0)
		.enable_validation_layers(enableValidationLayers)
		.use_default_debug_messenger()
		.enable_extensions(sdlExtCount, sdlInstanceExtensions)
		.build();

	handleBootstrapErrors(instance_ret);

	vkb::Instance vkbInstance = instance_ret.value();
	instance = vk::raii::Instance(context, vkbInstance.instance);
	debugMessenger = vk::raii::DebugUtilsMessengerEXT(instance,vkbInstance.debug_messenger);

	// ---- Create Surface

	VkSurfaceKHR _surface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(window, *instance, nullptr, &_surface)) {
		throw std::runtime_error(SDL_GetError());
	}
	surface = vk::raii::SurfaceKHR(instance, _surface);

	// ---- Select Physical Device

	vkb::PhysicalDeviceSelector selector{vkbInstance};
	auto phys_ret = selector
	.set_surface(*surface)
	.set_minimum_version(1,3)
	.add_required_extensions(deviceExtensions)
	.select();

	handleBootstrapErrors(phys_ret);

	const vkb::PhysicalDevice vkbPhysicalDevice = phys_ret.value();
	physicalDevice = vk::raii::PhysicalDevice(instance, vkbPhysicalDevice);
	msaaSamples = getMaxUsableSampleCount();

	// ---- Create Logical Device

	vk::PhysicalDeviceFeatures2 features{};
	features.features.samplerAnisotropy = VK_TRUE;
	features.features.sampleRateShading = VK_TRUE;

	vk::PhysicalDeviceVulkan13Features features13{};
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT featuresEXT{};
	featuresEXT.extendedDynamicState = VK_TRUE;

	vkb::DeviceBuilder device_builder{vkbPhysicalDevice};
	auto dev_ret = device_builder
	.add_pNext(&features)
	.add_pNext(&features13)
	.add_pNext(&featuresEXT)
	.build();

	handleBootstrapErrors(dev_ret);

	vkbDevice = dev_ret.value();
	device = vk::raii::Device(physicalDevice, vkbDevice.device);

	auto queue_ret = vkbDevice.get_queue_index(vkb::QueueType::graphics);
	graphicsQueue = vk::raii::Queue(device, queue_ret.value(), 0);
	presentQueue = vk::raii::Queue(device, queue_ret.value(), 0);

	// ---- Create Swapchain
	auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
	swapChainExtent = chooseSwapExtent(surfaceCapabilities);

	vkb::SwapchainBuilder swapchain_builder{vkbDevice};
	auto swap_ret = swapchain_builder
		.set_desired_extent(swapChainExtent.width, swapChainExtent.height)
		.build();

	handleBootstrapErrors(swap_ret);

	vkbSwapchain = swap_ret.value();

	swapChain = vk::raii::SwapchainKHR(device, vkbSwapchain.swapchain);
	swapChainImages = swapChain.getImages();
	swapChainImageFormat = static_cast<vk::Format>(vkbSwapchain.image_format);
}

vk::Extent2D Renderer::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}

	int width, height;
	SDL_GetWindowSizeInPixels(window, &width, &height);

	return {
		std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
		std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
		};
}

void Renderer::createImageViews() {
	swapChainImageViews.clear();

	vk::ImageViewCreateInfo imageViewCreateInfo{};
	imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
	imageViewCreateInfo.format = swapChainImageFormat;
	imageViewCreateInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

	for (auto image : swapChainImages) {
		imageViewCreateInfo.image = image;
		swapChainImageViews.emplace_back(device, imageViewCreateInfo);
	}
}

[[nodiscard]] vk::raii::ShaderModule Renderer::createShaderModule(const std::vector<char>& code) {
	vk::ShaderModuleCreateInfo createInfo;
	createInfo.codeSize = code.size() * sizeof(char);
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
	vk::raii::ShaderModule shaderModule(device, createInfo);
	return shaderModule;
}

void Renderer::createGraphicsPipeline() {
	auto shaderCode = readFile("C:/dev/vulkan-doc-tutorial/shaders/slang.spv");
	std::cout << "Size of shaderCode: " << shaderCode.size() << std::endl;
	vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	vertShaderStageInfo.module = shaderModule;
	vertShaderStageInfo.pName = "vertMain";

	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	fragShaderStageInfo.module = shaderModule;
	fragShaderStageInfo.pName = "fragMain";

	vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo,fragShaderStageInfo};

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	std::vector dynamicStates= {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	vk::PipelineDynamicStateCreateInfo dynamicState;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

	vk::Viewport{0.0f, 0.0, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f};
	vk::PipelineViewportStateCreateInfo viewportState({}, 1,{},1);

	vk::PipelineDepthStencilStateCreateInfo depthStencil = {};
	depthStencil.depthTestEnable = vk::True;
	depthStencil.depthWriteEnable = vk::True;
	depthStencil.depthCompareOp = vk::CompareOp::eLess;
	depthStencil.depthBoundsTestEnable = vk::False;
	depthStencil.stencilTestEnable = vk::False;

	vk::PipelineRasterizationStateCreateInfo rasterizer;
	rasterizer.depthClampEnable = vk::False;
	rasterizer.rasterizerDiscardEnable = vk::False;
	rasterizer.polygonMode = vk::PolygonMode::eFill;
	rasterizer.cullMode = vk::CullModeFlagBits::eBack;
	rasterizer.frontFace = vk::FrontFace::eCounterClockwise; // this needs to be counterClockwise since we are using GLM for our uniform buffers which is originally designed for OpenGL where Y-axis is flipped
	rasterizer.depthBiasEnable = vk::False;
	rasterizer.depthBiasSlopeFactor = 1.0f;
	rasterizer.lineWidth = 1.0f;

	vk::PipelineMultisampleStateCreateInfo multisampling;
	multisampling.rasterizationSamples = msaaSamples;
	multisampling.sampleShadingEnable = vk::True;
	multisampling.minSampleShading = 0.2f;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable = vk::False; // disabling color blending for now.
	colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	colorBlendAttachment.colorBlendOp = vk::BlendOp::eAdd;
	colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
	colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	colorBlendAttachment.alphaBlendOp = vk::BlendOp::eAdd;

	vk::PipelineColorBlendStateCreateInfo colorBlending;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = vk::LogicOp::eCopy;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments =  &colorBlendAttachment;

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &*descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

	vk::Format depthFormat = findDepthFormat();

	vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainImageFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;

	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.pNext = &pipelineRenderingCreateInfo;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = nullptr;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
}

void Renderer::transition_image_layout(
	vk::Image image,
	vk::ImageLayout oldLayout,
	vk::ImageLayout newLayout,
	vk::AccessFlags2 srcAccessMask,
	vk::AccessFlags2 dstAccessMask,
	vk::PipelineStageFlags2 srcStageMask,
	vk::PipelineStageFlags2 dstStageMask,
	vk::ImageAspectFlags image_aspect_flags) {

	vk::ImageMemoryBarrier2 barrier;
	barrier.srcStageMask = srcStageMask,
	barrier.srcAccessMask = srcAccessMask,
	barrier.dstStageMask = dstStageMask,
	barrier.dstAccessMask = dstAccessMask,
	barrier.oldLayout = oldLayout,
	barrier.newLayout = newLayout,
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
	barrier.image = image,

	barrier.subresourceRange.aspectMask = image_aspect_flags;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	vk::DependencyInfo dependencyInfo;
	dependencyInfo.dependencyFlags = {};
	dependencyInfo.imageMemoryBarrierCount = 1;
	dependencyInfo.pImageMemoryBarriers = &barrier;

	commandBuffers[currentFrame].pipelineBarrier2(dependencyInfo);
}

void Renderer::createSyncObjects() {
	presentCompleteSemaphores.clear();
	renderCompleteSemaphores.clear();
	inFlightFences.clear();

	vk::FenceCreateInfo fenceInfo;
	fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		inFlightFences.emplace_back(device, fenceInfo);
	}

	for (size_t i = 0; i < swapChainImages.size(); i++) {
		renderCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
	}
}

void Renderer::recreateSwapChain() {
	int width = 0, height = 0;
	SDL_GetWindowSizeInPixels(window, &width, &height);
	while (width == 0 || height == 0) {
		SDL_GetWindowSizeInPixels(window, &width, &height);
	}

	device.waitIdle();

	vkb::SwapchainBuilder swapchain_builder{vkbDevice};
	auto swap_ret = swapchain_builder.set_old_swapchain(vkbSwapchain).build();
	handleBootstrapErrors(swap_ret);

	cleanupSwapchain(); // needs to be cleaned up AFTER the build() since we are using the old swapchain as ref

	vkbSwapchain = swap_ret.value();
	swapChain = vk::raii::SwapchainKHR(device, vkbSwapchain.swapchain);
	swapChainImages = swapChain.getImages();

	createImageViews();
	createColorResources();
	createDepthResources();
}

void Renderer::cleanupSwapchain() {
	device.waitIdle();

	colorImageView = nullptr;
	depthImageView = nullptr;

	if (colorImage.image != VK_NULL_HANDLE) {destroyImage(allocator, colorImage);}
	if (depthImage.image != VK_NULL_HANDLE) {destroyImage(allocator, depthImage);}

	swapChainImageViews.clear();
	swapChain = nullptr;
}

void Renderer::createTextureImage() {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(TEXTURE_PATH, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	vk::DeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	AllocatedBuffer stagingBuffer = {};
	createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, true);

	void* data = nullptr;
	vmaMapMemory(allocator, stagingBuffer.allocation, &data);
	memcpy(data, pixels, imageSize);
	vmaUnmapMemory(allocator, stagingBuffer.allocation);

	stbi_image_free(pixels);

	createImage(texWidth,
		texHeight,
		mipLevels,
		VK_SAMPLE_COUNT_1_BIT,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		textureImage);

	transitionImageLayout(vk::Image(textureImage.image), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
	copyBufferToImage(vk::Buffer(stagingBuffer.buffer), vk::Image(textureImage.image), static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	generateMipmaps(vk::Image(textureImage.image),vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, mipLevels);

	destroyBuffer(allocator, stagingBuffer);
}

void Renderer::transitionImageLayout(const vk::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels) {
	auto commandBuffer = beginSingleTimeCommands();
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
	endSingleTimeCommands(commandBuffer);
}

void Renderer::copyBufferToImage(const vk::Buffer& buffer, vk::Image image, uint32_t width, uint32_t height) {
	vk::BufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
	region.imageOffset = vk::Offset3D{0, 0, 0};
	region.imageExtent = vk::Extent3D{width, height, 1};

	auto commandBuffer = beginSingleTimeCommands();
	commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
	endSingleTimeCommands(commandBuffer);
}

vk::raii::ImageView Renderer::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) {
	vk::ImageViewCreateInfo viewInfo{};
	viewInfo.image = image;
	viewInfo.viewType = vk::ImageViewType::e2D;
	viewInfo.format = format;
	viewInfo.subresourceRange = {aspectFlags, 0, mipLevels, 0, 1};

	return vk::raii::ImageView(device, viewInfo);
}

void Renderer::createTextureImageView() {
	textureImageView = createImageView(vk::Image(textureImage.image), vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels);
}

void Renderer::createTextureSampler() {
	vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();

	vk::SamplerCreateInfo samplerInfo = {};
	samplerInfo.magFilter = vk::Filter::eLinear;
	samplerInfo.minFilter = vk::Filter::eLinear;

	samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = vk::LodClampNone;

	samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
	samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
	samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;

	samplerInfo.anisotropyEnable = vk::True;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

	samplerInfo.compareEnable = vk::False;
	samplerInfo.compareOp = vk::CompareOp::eAlways;

	samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;

	samplerInfo.unnormalizedCoordinates = vk::False;

	textureSampler = vk::raii::Sampler(device, samplerInfo);
}

void Renderer::createDepthResources() {
	vk::Format depthFormat = findDepthFormat();

	createImage(
		swapChainExtent.width,
		swapChainExtent.height,
		1,
		static_cast<VkSampleCountFlagBits>(msaaSamples),
		static_cast<VkFormat>(depthFormat),
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		depthImage);

	depthImageView = createImageView(vk::Image(depthImage.image), depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
}

vk::Format Renderer::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
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

vk::Format Renderer::findDepthFormat() {
	return findSupportedFormat(
	  {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
	  vk::ImageTiling::eOptimal,
	  vk::FormatFeatureFlagBits::eDepthStencilAttachment
	);
}

bool Renderer::hasStencilComponent(vk::Format format) {
	return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

/*
void Renderer::loadModel() {
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH)) {
		throw std::runtime_error(warn + err);
	}

	std::unordered_map<Vertex, uint32_t, VertexHasher> uniqueVertices{};

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex{};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2],
			};

			vertex.texCoord = {
				attrib.texcoords[2 * index.texcoord_index + 0],
				1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
			};

			vertex.color = {1.0f, 1.0f, 1.0f};

			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
				vertices.push_back(vertex);
			}

			indices.push_back(uniqueVertices[vertex]);
		}
	}
}
*/

void Renderer::loadModel() {
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, MODEL_PATH);

	if (!warn.empty()) {
		std::cout << "glTF warning: " << warn << std::endl;
	}

	if (!err.empty()) {
		std::cout << "glTF error: " << err << std::endl;
	}

	if (!ret) {
		throw std::runtime_error("failed to load glTF model");
	}

	// Process all meshes in the model
	std::unordered_map<Vertex, uint32_t, VertexHasher> uniqueVertices{};

	for (const auto& mesh : model.meshes) {
		for (const auto& primitive : mesh.primitives) {
			// Get indices
			const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
			const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
			const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

			// Get vertex positions
			const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
			const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
			const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

			// Get texture coordinates if available
			bool hasTexCoords = primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end();
			const tinygltf::Accessor* texCoordAccessor = nullptr;
			const tinygltf::BufferView* texCoordBufferView = nullptr;
			const tinygltf::Buffer* texCoordBuffer = nullptr;

			if (hasTexCoords) {
				texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
				texCoordBufferView = &model.bufferViews[texCoordAccessor->bufferView];
				texCoordBuffer = &model.buffers[texCoordBufferView->buffer];
			}

			// Process vertices
			for (size_t i = 0; i < posAccessor.count; i++) {
				Vertex vertex{};

				// Get position
				const float* pos = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + 1 * 12]);
				vertex.pos = {pos[0], pos[1], pos[2]};

				// Get texture coordinates if available
				if (hasTexCoords) {
					const float* texCoord = reinterpret_cast<const float*>(&texCoordBuffer->data[texCoordBufferView->byteOffset + texCoordAccessor->byteOffset + 1 * 8]);
					vertex.texCoord = {texCoord[0], 1.0f - texCoord[1]};
				} else {
					vertex.texCoord = {0.0f, 0.0f};
				}

				// Set default color
				vertex.color = {1.0f, 1.0f, 1.0f};

				// Add vertex if unique
				if (!uniqueVertices.contains(vertex)) {
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
				}
			}

			// Process indices
			const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];

			// Handle different index component types
			if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
				const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(indexData);
				for (size_t i = 0; i < indexAccessor.count; i++) {
					Vertex vertex = vertices[indices16[i]];
					indices.push_back(uniqueVertices[vertex]);
				}
			} else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
				const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(indexData);
				for (size_t i = 0; i < indexAccessor.count; i++) {
					Vertex vertex = vertices[indices32[i]];
					indices.push_back(uniqueVertices[vertex]);
				}
			} else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
				const uint8_t* indices8 = reinterpret_cast<const uint8_t*>(indexData);
				for (size_t i = 0; i < indexAccessor.count; i++) {
					Vertex vertex = vertices[indices8[i]];
					indices.push_back(uniqueVertices[vertex]);
				}
			}
		}
	}
}

void Renderer::generateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {

	// Check if image format supports linear blit-ing
	vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(imageFormat);

	if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
		throw std::runtime_error("Texture image format does not support linear filtering");
	}

	vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();

	vk::ImageMemoryBarrier barrier(
		vk::AccessFlagBits::eTransferWrite,
		vk::AccessFlagBits::eTransferRead,
		vk::ImageLayout::eTransferDstOptimal,
		vk::ImageLayout::eTransferSrcOptimal,
		vk::QueueFamilyIgnored,
		vk::QueueFamilyIgnored,
		image);

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
	endSingleTimeCommands(commandBuffer);

}

vk::SampleCountFlagBits Renderer::getMaxUsableSampleCount() {
	vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice.getProperties();

	vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
	if (counts & vk::SampleCountFlagBits::e64) {return vk::SampleCountFlagBits::e64;}
	if (counts & vk::SampleCountFlagBits::e32) {return vk::SampleCountFlagBits::e32;}
	if (counts & vk::SampleCountFlagBits::e16) {return vk::SampleCountFlagBits::e16;}
	if (counts & vk::SampleCountFlagBits::e8) {return vk::SampleCountFlagBits::e8;}
	if (counts & vk::SampleCountFlagBits::e4) {return vk::SampleCountFlagBits::e4;}
	if (counts & vk::SampleCountFlagBits::e2) {return vk::SampleCountFlagBits::e2;}

	return vk::SampleCountFlagBits::e1;
}

void Renderer::createColorResources() {
	vk::Format colorFormat = swapChainImageFormat;

	createImage(
		swapChainExtent.width,
		swapChainExtent.height,
		1,
		static_cast<VkSampleCountFlagBits>(msaaSamples),
		static_cast<VkFormat>(colorFormat),
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		colorImage);

	colorImageView = createImageView(vk::Image(colorImage.image), colorFormat, vk::ImageAspectFlagBits::eColor, 1);
}

void Renderer::initVulkan() {
	bootstrapVulkan();
	createImageViews();
	createAllocator();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createCommandPool();
	createColorResources();
	createDepthResources();
	createTextureImage();
	createTextureImageView();
	createTextureSampler();
	loadModel();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createCommandBuffers();
	createSyncObjects();
}

void Renderer::mainLoop() {
	bool running = true;
	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
				case SDL_EVENT_QUIT:
					running = false;
					break;
				case SDL_EVENT_WINDOW_RESIZED:
				case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
					framebufferResized = true;
					break;
			}
		}
		drawFrame();
	}

	device.waitIdle();
}

void Renderer::dumpAllocationStats() {
	char* stats = nullptr;
	vmaBuildStatsString(allocator, &stats, VK_TRUE);
	std::cerr << stats << std::endl;
	vmaFreeStatsString(allocator, stats);
}

void Renderer::cleanup() {

	SDL_DestroyWindow(window);
	SDL_Quit();

	for (auto& ub : uniformBuffers) { destroyBuffer(allocator, ub.buffer); }
	destroyBuffer(allocator, vertexBuffer);
	destroyBuffer(allocator, indexBuffer);
	destroyImage(allocator, textureImage);
	destroyImage(allocator, depthImage);
	destroyImage(allocator, colorImage);
	//dumpAllocationStats();
	destroyAllocator();
}


// https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html
// fix for the validation errors caused by semaphore reuse
void Renderer::drawFrame() {
	auto fenceResult = device.waitForFences(*inFlightFences[currentFrame], vk::True, UINT64_MAX);

	auto [result, imageIndex] = swapChain.acquireNextImage(
		UINT64_MAX,
		*presentCompleteSemaphores[currentFrame],
		nullptr);

	if (result == vk::Result::eErrorOutOfDateKHR) {
		recreateSwapChain();
		return;
	}

	if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("failed to acquire swap chain image");
	}

	device.resetFences(*inFlightFences[currentFrame]); // this is only performed after we handle the return values of .acquireNextImageKHR()!

	commandBuffers[currentFrame].reset();
	recordCommandBuffer(imageIndex);

	vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

	updateUniformBuffer(currentFrame);

	vk::SubmitInfo submitInfo;

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &*presentCompleteSemaphores[currentFrame];
	submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
	submitInfo.pWaitDstStageMask = &waitDestinationStageMask;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &*commandBuffers[currentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &*renderCompleteSemaphores[imageIndex];

	graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);

	const vk::PresentInfoKHR presentInfoKHR( *renderCompleteSemaphores[imageIndex], *swapChain, imageIndex);

	result = presentQueue.presentKHR(presentInfoKHR);

	if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
		framebufferResized = false;
		recreateSwapChain();
	} else if (result != vk::Result::eSuccess) {
		throw std::runtime_error("failed to present swap chain image");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

