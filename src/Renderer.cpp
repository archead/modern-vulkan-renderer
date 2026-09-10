#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#endif
#include <iostream>
#include <stdexcept>
#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <chrono>
#include <unordered_map> // Using for deduplicating OBJ vertices
#include <fstream>
#include <memory>


#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// #define TINYGLTF_IMPLEMENTATION
// #define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include "Renderer.hpp"
#include <vk_mem_alloc.h>

#include "Types.hpp"
#include "glm/gtc/type_ptr.inl"

#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl3.h>

#include "DescriptorWriter.hpp"
#include "VkUtil.hpp"
#include "MikkUtil.hpp"

static void check_vk_result(VkResult err) {
	if (err == 0) { return; }
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0){ abort(); }
}

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

void Renderer::loadModels() {

}

glm::mat4 Renderer::GameObject::getModelMatrix() const {
	auto model = glm::mat4(1.0f);
	model = glm::translate(model, position);
	model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::scale(model, scale);
	return model;
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
		.set_app_name("Modern Renderer")
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
	std::cout << "Physical Device Push Constant Size: " << physicalDevice.getProperties().limits.maxPushConstantsSize << "\n";
	msaaSamples = vk::SampleCountFlagBits::e1; // hard coded since we are using deferred rendering

	// ---- Create Logical Device

	vk::PhysicalDeviceFeatures2 features{};
	features.features.samplerAnisotropy = VK_TRUE;
	features.features.sampleRateShading = VK_TRUE;

	vk::PhysicalDeviceVulkan13Features features13{};
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;

	vk::PhysicalDeviceVulkan11Features features11{};
	features11.shaderDrawParameters = VK_TRUE;

	vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT featuresEXT{};
	featuresEXT.extendedDynamicState = VK_TRUE;

	vkb::DeviceBuilder device_builder{vkbPhysicalDevice};
	auto dev_ret = device_builder
	.add_pNext(&features)
	.add_pNext(&features11)
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
	std::cout << "supportedUsageFlags = " << vk::to_string(surfaceCapabilities.supportedUsageFlags) << "\n";
	swapChainExtent = chooseSwapExtent(surfaceCapabilities);

	vkb::SwapchainBuilder swapchain_builder{vkbDevice};
	auto swap_ret = swapchain_builder
		.set_desired_extent(swapChainExtent.width, swapChainExtent.height)
		.set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build();

	handleBootstrapErrors(swap_ret);

	vkbSwapchain = swap_ret.value();

	swapChain = vk::raii::SwapchainKHR(device, vkbSwapchain.swapchain);
	swapChainImages = swapChain.getImages();
	swapChainImageFormat = static_cast<vk::Format>(vkbSwapchain.image_format);
}

vk::Extent2D Renderer::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
	if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
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

void Renderer::createLightingPipeline() {
	auto shaderCode = readFile("C:/dev/vulkan-doc-tutorial/shaders/slang.spv");
	std::cout << "Size of shaderCode: " << shaderCode.size() << std::endl;
	vk::raii::ShaderModule shaderModule = vkutil::createShaderModule(device, shaderCode);

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	vertShaderStageInfo.stage  = vk::ShaderStageFlagBits::eVertex;
	vertShaderStageInfo.module = shaderModule;
	vertShaderStageInfo.pName  = "lightingVertMain";

	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	fragShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
	fragShaderStageInfo.module = shaderModule;
	fragShaderStageInfo.pName  = "lightingFragMain";

	vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.vertexBindingDescriptionCount                = 0;
	vertexInputInfo.pVertexBindingDescriptions                   = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount              = 0;
	vertexInputInfo.pVertexAttributeDescriptions                 = nullptr;

	std::vector dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

	vk::PipelineDynamicStateCreateInfo dynamicState;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates    = dynamicStates.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

	vk::PipelineViewportStateCreateInfo viewportState({}, 1, {}, 1);

	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	depthStencil.depthTestEnable       = vk::False;
	depthStencil.depthWriteEnable      = vk::False;

	vk::PipelineRasterizationStateCreateInfo rasterizer;
	rasterizer.depthClampEnable        = vk::False;
	rasterizer.rasterizerDiscardEnable = vk::False;
	rasterizer.polygonMode             = vk::PolygonMode::eFill;
	rasterizer.cullMode                = vk::CullModeFlagBits::eNone; // disabled for the big trangle
	rasterizer.depthBiasEnable      = vk::False;
	rasterizer.depthBiasSlopeFactor = 1.0f;
	rasterizer.lineWidth            = 1.0f;

	vk::PipelineMultisampleStateCreateInfo multisample;
	multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable         = vk::False; // disabled for Deferred rendering

	std::array<vk::PipelineColorBlendAttachmentState, 1> colorBlendAttachments = {};
	colorBlendAttachments.fill(colorBlendAttachment);

	vk::PipelineColorBlendStateCreateInfo colorBlending;
	colorBlending.logicOpEnable   = VK_FALSE;
	colorBlending.logicOp         = vk::LogicOp::eCopy;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments    = colorBlendAttachments.data();

	vk::PushConstantRange range = {};
	range.stageFlags = vk::ShaderStageFlagBits::eFragment;
	range.offset = 0;
	range.size = 4;

	std::array<vk::DescriptorSetLayout, 1> setLayouts = {*gBufferSetLayout};
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipelineLayoutInfo.pSetLayouts    = setLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &range;

	lightingPipelineLayout                    = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

	vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
	pipelineRenderingCreateInfo.colorAttachmentCount    = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainImageFormat;

	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.pNext               = &pipelineRenderingCreateInfo;
	pipelineInfo.stageCount          = 2;
	pipelineInfo.pStages             = shaderStages;
	pipelineInfo.pVertexInputState   = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState      = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pDepthStencilState  = &depthStencil;
	pipelineInfo.pMultisampleState   = &multisample;
	pipelineInfo.pColorBlendState    = &colorBlending;
	pipelineInfo.pDynamicState       = &dynamicState;
	pipelineInfo.layout              = lightingPipelineLayout;
	pipelineInfo.renderPass          = nullptr;
	pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex   = -1;

	lightingPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
}

void Renderer::createGeometryPipeline() {
	auto shaderCode = readFile("C:/dev/vulkan-doc-tutorial/shaders/slang.spv");
	std::cout << "Size of shaderCode: " << shaderCode.size() << std::endl;
	vk::raii::ShaderModule shaderModule = vkutil::createShaderModule(device, shaderCode);

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	vertShaderStageInfo.stage  = vk::ShaderStageFlagBits::eVertex;
	vertShaderStageInfo.module = shaderModule;
	vertShaderStageInfo.pName  = "vertMain";

	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	fragShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
	fragShaderStageInfo.module = shaderModule;
	fragShaderStageInfo.pName  = "geometryFragMain";

	vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	auto                                   bindingDescription    = Vertex::getBindingDescription();
	auto                                   attributeDescriptions = Vertex::getAttributeDescriptions();
	vertexInputInfo.vertexBindingDescriptionCount                = 1;
	vertexInputInfo.pVertexBindingDescriptions                   = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount              = attributeDescriptions.size();
	vertexInputInfo.pVertexAttributeDescriptions                 = attributeDescriptions.data();

	std::vector dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	vk::PipelineDynamicStateCreateInfo dynamicState;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates    = dynamicStates.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

	vk::PipelineViewportStateCreateInfo viewportState({}, 1, {}, 1);

	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	depthStencil.depthTestEnable       = vk::True;
	depthStencil.depthWriteEnable      = vk::True;
	depthStencil.depthCompareOp        = vk::CompareOp::eLess;
	depthStencil.depthBoundsTestEnable = vk::False;
	depthStencil.stencilTestEnable     = vk::False;

	vk::PipelineRasterizationStateCreateInfo rasterizer;
	rasterizer.depthClampEnable        = vk::False;
	rasterizer.rasterizerDiscardEnable = vk::False;
	rasterizer.polygonMode             = vk::PolygonMode::eFill;
	rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
	rasterizer.frontFace               = vk::FrontFace::eCounterClockwise;
	// this needs to be counterClockwise since we are using GLM for our uniform buffers which is originally designed for OpenGL where Y-axis is flipped
	rasterizer.depthBiasEnable      = vk::False;
	rasterizer.depthBiasSlopeFactor = 1.0f;
	rasterizer.lineWidth            = 1.0f;

	vk::PipelineMultisampleStateCreateInfo multisample;
	multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable         = vk::False; // disabled for Deferred rendering

	std::array<vk::PipelineColorBlendAttachmentState, 3> colorBlendAttachments = {};
	colorBlendAttachments.fill(colorBlendAttachment);

	vk::PipelineColorBlendStateCreateInfo colorBlending;
	colorBlending.logicOpEnable   = VK_FALSE;
	colorBlending.logicOp         = vk::LogicOp::eCopy;
	colorBlending.attachmentCount = 3;
	colorBlending.pAttachments    = colorBlendAttachments.data();

	std::array<vk::DescriptorSetLayout, 2> setLayouts = {*globalSetLayout, *objectSetLayout};

	vk::PushConstantRange objectPushConstantRange{};
	objectPushConstantRange.size = sizeof(ObjectUBO);
	objectPushConstantRange.offset = 0;
	objectPushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex;

	vk::PipelineLayoutCreateInfo           pipelineLayoutInfo;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipelineLayoutInfo.pSetLayouts    = setLayouts.data();
	pipelineLayoutInfo.pPushConstantRanges = &objectPushConstantRange;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	geometryPipelineLayout            = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

	vk::Format depthFormat = vkutil::findDepthFormat(physicalDevice);
	std::array<vk::Format, 3> gBufferFormats = {gBuffer.fragPosFormat, gBuffer.normalVectorFormat, gBuffer.albedoColorFormat};

	vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
	pipelineRenderingCreateInfo.colorAttachmentCount    = 3;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = gBufferFormats.data();
	pipelineRenderingCreateInfo.depthAttachmentFormat   = depthFormat;

	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.pNext               = &pipelineRenderingCreateInfo;
	pipelineInfo.stageCount          = 2;
	pipelineInfo.pStages             = shaderStages;
	pipelineInfo.pVertexInputState   = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState      = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pDepthStencilState  = &depthStencil;
	pipelineInfo.pMultisampleState   = &multisample;
	pipelineInfo.pColorBlendState    = &colorBlending;
	pipelineInfo.pDynamicState       = &dynamicState;
	pipelineInfo.layout              = geometryPipelineLayout;
	pipelineInfo.renderPass          = nullptr;
	pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex   = -1;

	geometryPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
}

void Renderer::createBillboardPipeline() {

	auto shaderCode = readFile("C:/dev/vulkan-doc-tutorial/shaders/slang.spv");
	std::cout << "Size of shaderCode: " << shaderCode.size() << std::endl;
	vk::raii::ShaderModule shaderModule = vkutil::createShaderModule(device, shaderCode);

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	vertShaderStageInfo.stage  = vk::ShaderStageFlagBits::eVertex;
	vertShaderStageInfo.module = shaderModule;
	vertShaderStageInfo.pName  = "billboardVertMain";

	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	fragShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
	fragShaderStageInfo.module = shaderModule;
	fragShaderStageInfo.pName  = "billboardFragMain";

	vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	vertexInputInfo.vertexBindingDescriptionCount                = 0;
	vertexInputInfo.pVertexBindingDescriptions                   = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount              = 0;
	vertexInputInfo.pVertexAttributeDescriptions                 = nullptr;

	std::vector dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

	vk::PipelineDynamicStateCreateInfo dynamicState;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates    = dynamicStates.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

	vk::PipelineViewportStateCreateInfo viewportState({}, 1, {}, 1);

	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	depthStencil.depthTestEnable       = vk::False;
	depthStencil.depthWriteEnable      = vk::False;

	vk::PipelineRasterizationStateCreateInfo rasterizer;
	rasterizer.depthClampEnable        = vk::False;
	rasterizer.rasterizerDiscardEnable = vk::False;
	rasterizer.polygonMode             = vk::PolygonMode::eFill;
	rasterizer.cullMode                = vk::CullModeFlagBits::eNone; // disabled for the big trangle
	rasterizer.depthBiasEnable      = vk::False;
	rasterizer.depthBiasSlopeFactor = 1.0f;
	rasterizer.lineWidth            = 1.0f;

	vk::PipelineMultisampleStateCreateInfo multisample;
	multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable         = vk::False; // current dont need this as we are outputting a solid color

	std::array<vk::PipelineColorBlendAttachmentState, 1> colorBlendAttachments = {};
	colorBlendAttachments.fill(colorBlendAttachment);

	vk::PipelineColorBlendStateCreateInfo colorBlending;
	colorBlending.logicOpEnable   = VK_FALSE;
	colorBlending.logicOp         = vk::LogicOp::eCopy;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments    = colorBlendAttachments.data();

	std::array<vk::DescriptorSetLayout, 1> descriptorSetLayouts = {billboardSetLayout};

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.setLayoutCount         = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutInfo.pSetLayouts            = descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges    = nullptr;

	billboardPipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

	vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
	pipelineRenderingCreateInfo.colorAttachmentCount    = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainImageFormat;

	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.pNext               = &pipelineRenderingCreateInfo;
	pipelineInfo.stageCount          = 2;
	pipelineInfo.pStages             = shaderStages;
	pipelineInfo.pVertexInputState   = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState      = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pDepthStencilState  = &depthStencil;
	pipelineInfo.pMultisampleState   = &multisample;
	pipelineInfo.pColorBlendState    = &colorBlending;
	pipelineInfo.pDynamicState       = &dynamicState;
	pipelineInfo.layout              = billboardPipelineLayout;
	pipelineInfo.renderPass          = nullptr;
	pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex   = -1;

	billboardPipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
}

void Renderer::createGraphicsPipeline() {
	auto shaderCode = readFile("C:/dev/vulkan-doc-tutorial/shaders/slang.spv");
	std::cout << "Size of shaderCode: " << shaderCode.size() << std::endl;
	vk::raii::ShaderModule shaderModule = vkutil::createShaderModule(device, shaderCode);

	vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
	vertShaderStageInfo.stage  = vk::ShaderStageFlagBits::eVertex;
	vertShaderStageInfo.module = shaderModule;
	vertShaderStageInfo.pName  = "vertMain";

	vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
	fragShaderStageInfo.stage  = vk::ShaderStageFlagBits::eFragment;
	fragShaderStageInfo.module = shaderModule;
	fragShaderStageInfo.pName  = "fragMain";

	vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
	auto                                   bindingDescription    = Vertex::getBindingDescription();
	auto                                   attributeDescriptions = Vertex::getAttributeDescriptions();
	vertexInputInfo.vertexBindingDescriptionCount                = 1;
	vertexInputInfo.pVertexBindingDescriptions                   = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount              = attributeDescriptions.size();
	vertexInputInfo.pVertexAttributeDescriptions                 = attributeDescriptions.data();

	std::vector dynamicStates = {
		vk::DynamicState::eViewport,
		vk::DynamicState::eScissor
	};

	vk::PipelineDynamicStateCreateInfo dynamicState;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates    = dynamicStates.data();

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;

	vk::Viewport{ 0.0f, 0.0, static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height), 0.0f, 1.0f };
	vk::PipelineViewportStateCreateInfo viewportState({}, 1, {}, 1);

	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	depthStencil.depthTestEnable       = vk::True;
	depthStencil.depthWriteEnable      = vk::True;
	depthStencil.depthCompareOp        = vk::CompareOp::eLess;
	depthStencil.depthBoundsTestEnable = vk::False;
	depthStencil.stencilTestEnable     = vk::False;

	vk::PipelineRasterizationStateCreateInfo rasterizer;
	rasterizer.depthClampEnable        = vk::False;
	rasterizer.rasterizerDiscardEnable = vk::False;
	rasterizer.polygonMode             = vk::PolygonMode::eFill;
	rasterizer.cullMode                = vk::CullModeFlagBits::eBack;
	rasterizer.frontFace               = vk::FrontFace::eCounterClockwise;
	// this needs to be counterClockwise since we are using GLM for our uniform buffers which is originally designed for OpenGL where Y-axis is flipped
	rasterizer.depthBiasEnable      = vk::False;
	rasterizer.depthBiasSlopeFactor = 1.0f;
	rasterizer.lineWidth            = 1.0f;

	vk::PipelineMultisampleStateCreateInfo multisampling;
	multisampling.rasterizationSamples = msaaSamples;
	multisampling.sampleShadingEnable  = vk::True;
	multisampling.minSampleShading     = 0.2f;

	vk::PipelineColorBlendAttachmentState colorBlendAttachment;
	colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
										  vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendAttachment.blendEnable         = vk::False; // disabling color blending for now.
	colorBlendAttachment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	colorBlendAttachment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	colorBlendAttachment.colorBlendOp        = vk::BlendOp::eAdd;
	colorBlendAttachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
	colorBlendAttachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	colorBlendAttachment.alphaBlendOp        = vk::BlendOp::eAdd;

	vk::PipelineColorBlendStateCreateInfo colorBlending;
	colorBlending.logicOpEnable   = VK_FALSE;
	colorBlending.logicOp         = vk::LogicOp::eCopy;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments    = &colorBlendAttachment;

	std::array<vk::DescriptorSetLayout, 2> setLayouts = { *globalSetLayout, *objectSetLayout};

	vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipelineLayoutInfo.pSetLayouts    = setLayouts.data();
	pipelineLayout                    = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

	vk::Format depthFormat = vkutil::findDepthFormat(physicalDevice);

	vk::PipelineRenderingCreateInfo pipelineRenderingCreateInfo;
	pipelineRenderingCreateInfo.colorAttachmentCount    = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &swapChainImageFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat   = depthFormat;

	vk::GraphicsPipelineCreateInfo pipelineInfo;
	pipelineInfo.pNext               = &pipelineRenderingCreateInfo;
	pipelineInfo.stageCount          = 2;
	pipelineInfo.pStages             = shaderStages;
	pipelineInfo.pVertexInputState   = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState      = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pDepthStencilState  = &depthStencil;
	pipelineInfo.pMultisampleState   = &multisampling;
	pipelineInfo.pColorBlendState    = &colorBlending;
	pipelineInfo.pDynamicState       = &dynamicState;
	pipelineInfo.layout              = pipelineLayout;
	pipelineInfo.renderPass          = nullptr;
	pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex   = -1;

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
	while (width == 0 || height == 0) { SDL_GetWindowSizeInPixels(window, &width, &height); }

	device.waitIdle();

	vkb::SwapchainBuilder swapchain_builder{vkbDevice};
	auto swap_ret = swapchain_builder
	.set_old_swapchain(vkbSwapchain)
	.set_desired_extent(width, height)
	.set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	.build();

	handleBootstrapErrors(swap_ret);

	cleanupSwapchain(); // needs to be cleaned up AFTER the build() since we are using the old swapchain as ref

	vkbSwapchain = swap_ret.value();

	swapChainExtent = vk::Extent2D(width, height);

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

	if (colorImage.image != VK_NULL_HANDLE) {vkutil::destroyImage(allocator, colorImage);}
	if (depthImage.image != VK_NULL_HANDLE) {vkutil::destroyImage(allocator, depthImage);}

	swapChainImageViews.clear();
	swapChain = nullptr;
}

void Renderer::createTextureImageView() {
	textureImageView = vkutil::createImageView(device, vk::Image(textureImage.image), textureFormat, vk::ImageAspectFlagBits::eColor, mipLevels);
}

void Renderer::createDepthResources() {
	vk::Format depthFormat = vkutil::findDepthFormat(physicalDevice);

	vkutil::createImage(allocator, swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, static_cast<VkFormat>(depthFormat), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImage);

	depthImageView = vkutil::createImageView(device, vk::Image(depthImage.image), depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
}


bool Renderer::hasStencilComponent(vk::Format format) {
	return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

void Renderer::loadModelGLTF(std::string modelPath) {
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	std::cout << "Loading Model: " << modelPath << std::endl;
	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, modelPath);

	if (!warn.empty())	{ std::cout << "glTF warning: " << warn << std::endl; }
	if (!err.empty())	{ std::cout << "glTF error: " << err << std::endl; }
	if (!ret)			{ throw std::runtime_error("failed to load glTF model"); }

	// Process all meshes in the model
	std::unordered_map<Vertex, uint32_t, VertexHasher> uniqueVertices{};

	for (const auto& mesh : model.meshes) {
		for (const auto& primitive : mesh.primitives) {
			if (primitive.indices == -1) { continue; } // doesn't handle cases where there is no index data but should prevent out of range vector reads

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

			// Get normals if available
			bool hasNormals = primitive.attributes.find("NORMAL") != primitive.attributes.end();
			const tinygltf::Accessor* normalAccessor = nullptr;
			const tinygltf::BufferView* normalBufferView = nullptr;
			const tinygltf::Buffer* normalBuffer = nullptr;

			if (hasTexCoords) {
				texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
				texCoordBufferView = &model.bufferViews[texCoordAccessor->bufferView];
				texCoordBuffer = &model.buffers[texCoordBufferView->buffer];
			}

			if (hasNormals) {
				normalAccessor = &model.accessors[primitive.attributes.at("NORMAL")];
				normalBufferView = &model.bufferViews[normalAccessor->bufferView];
				normalBuffer = &model.buffers[normalBufferView->buffer];
			}

			std::vector<uint32_t> remap(posAccessor.count); // used to deduplicate indices as well

			// Process vertices
			for (size_t i = 0; i < posAccessor.count; i++) {
				Vertex vertex{};

				// Get position
				const float* pos = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + i * 12]);
				vertex.pos = {pos[0], pos[1], pos[2]};

				// Get texture coordinates if available
				if (hasTexCoords) {
					const float* texCoord = reinterpret_cast<const float*>(&texCoordBuffer->data[texCoordBufferView->byteOffset + texCoordAccessor->byteOffset + i * 8]);
					vertex.texCoord = {texCoord[0], texCoord[1]};
				} else { vertex.texCoord = {0.0f, 0.0f}; }

				// Set default color
				vertex.color = {1.0f, 1.0f, 1.0f};

				if (hasNormals) {
					const float* normal = reinterpret_cast<const float*>(&normalBuffer->data[normalBufferView->byteOffset + normalAccessor->byteOffset + i * 12]);
					vertex.normal = {normal[0], normal[1], normal[2]};
				} else { vertex.normal = {0.0f, 0.0f, 0.0f}; }

				// Add vertex if unique
				if (!uniqueVertices.contains(vertex)) {
					remap[i] = static_cast<uint32_t>(vertices.size());
					uniqueVertices[vertex] = remap[i];
					vertices.push_back(vertex);
				} else {
					remap[i] = uniqueVertices[vertex];
				}
			}

			// Process indices
			const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];

			// Handle different index component types
			if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
				const auto* indices16 = reinterpret_cast<const uint16_t*>(indexData);
				for (size_t i = 0; i < indexAccessor.count; i++) { indices.push_back(remap[indices16[i]]); }
			}
			else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
				const auto* indices32 = reinterpret_cast<const uint32_t*>(indexData);
				for (size_t i = 0; i < indexAccessor.count; i++) { indices.push_back(remap[indices32[i]]); }
			}
			else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
				const auto* indices8 = reinterpret_cast<const uint8_t*>(indexData);
				for (size_t i = 0; i < indexAccessor.count; i++) { indices.push_back(remap[indices8[i]]); }
			}
		}
	}
	mikkutil::unweldVertices(vertices, indices);
	mikkutil::generateTangents(vertices, indices);
}

void Renderer::createColorResources() {
	vk::Format colorFormat = swapChainImageFormat;

	vkutil::createImage(allocator, swapChainExtent.width, swapChainExtent.height, 1, static_cast<VkSampleCountFlagBits>(msaaSamples), static_cast<VkFormat>(colorFormat), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, colorImage);
	colorImageView = vkutil::createImageView(device, vk::Image(colorImage.image), colorFormat, vk::ImageAspectFlagBits::eColor, 1);
}

void Renderer::createGBuffer() {
	vkutil::createImage(allocator, swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, static_cast<VkFormat>(gBuffer.fragPosFormat), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, gBuffer.fragPosImage);
	vkutil::createImage(allocator, swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, static_cast<VkFormat>(gBuffer.normalVectorFormat), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, gBuffer.normalVectorImage);
	vkutil::createImage(allocator, swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, static_cast<VkFormat>(gBuffer.albedoColorFormat), VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, gBuffer.albedoColorImage);

	gBuffer.fragPosImageView = vkutil::createImageView(device, gBuffer.fragPosImage.image, gBuffer.fragPosFormat, vk::ImageAspectFlagBits::eColor, 1);
	gBuffer.normalVectorImageView = vkutil::createImageView(device, gBuffer.normalVectorImage.image, gBuffer.normalVectorFormat, vk::ImageAspectFlagBits::eColor, 1);
	gBuffer.albedoColorImageView = vkutil::createImageView(device, gBuffer.albedoColorImage.image, gBuffer.albedoColorFormat, vk::ImageAspectFlagBits::eColor, 1);
}

void Renderer::recreateGBuffer() {
	vkutil::destroyImage(allocator, gBuffer.albedoColorImage);
	vkutil::destroyImage(allocator, gBuffer.normalVectorImage);
	vkutil::destroyImage(allocator, gBuffer.fragPosImage);

	createGBuffer();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		DescriptorWriter{}
    	.writeImage(0, *gBufferSampler, gBuffer.fragPosImageView, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
    	.writeImage(1, *gBufferSampler, gBuffer.normalVectorImageView, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
    	.writeImage(2, *gBufferSampler, gBuffer.albedoColorImageView, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
    	.updateSet(device, gBufferDescriptorSets[i]);
    }
}

void Renderer::createGBufferSampler() {
	vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
	vk::SamplerCreateInfo samplerInfo   = {};
	samplerInfo.magFilter               = vk::Filter::eNearest;
	samplerInfo.minFilter               = vk::Filter::eNearest;
	samplerInfo.mipmapMode              = vk::SamplerMipmapMode::eNearest;
	samplerInfo.mipLodBias              = 0.0f;
	samplerInfo.minLod                  = 0.0f;
	samplerInfo.maxLod                  = vk::LodClampNone;
	samplerInfo.addressModeU            = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeV            = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.addressModeW            = vk::SamplerAddressMode::eClampToEdge;
	samplerInfo.anisotropyEnable        = vk::False;
	samplerInfo.compareEnable           = vk::False;
	samplerInfo.compareOp               = vk::CompareOp::eAlways;
	samplerInfo.borderColor             = vk::BorderColor::eIntOpaqueBlack;
	samplerInfo.unnormalizedCoordinates = vk::False;

	gBufferSampler = vk::raii::Sampler(device, samplerInfo);
}

void Renderer::createGameObjects() {

	models.emplace_back(device, commandPool, graphicsQueue, &allocator, R"(C:\dev\vulkan-doc-tutorial\models\helmet\DamagedHelmet.gltf)");
	modelTextures.emplace_back(loadTextureKTX(R"(C:\dev\vulkan-doc-tutorial\textures\helmet\albedo.ktx2)"));
	modelTextures.emplace_back(loadTextureKTX(R"(C:\dev\vulkan-doc-tutorial\textures\helmet\normal.ktx2)"));

	models.emplace_back(device, commandPool, graphicsQueue, &allocator, R"(C:\dev\vulkan-doc-tutorial\models\fish\BarramundiFish.gltf)");
	modelTextures.emplace_back(loadTextureKTX(R"(C:\dev\vulkan-doc-tutorial\textures\fish\albedo.ktx2)"));
	modelTextures.emplace_back(loadTextureKTX(R"(C:\dev\vulkan-doc-tutorial\textures\fish\normal.ktx2)"));

	gameObjects.resize(2);

	gameObjects[0].modelIndex = 0;
	gameObjects[0].position = {0.0f, 0.0f, 0.0f};
	gameObjects[0].scale    = {1.0f, 1.0f, 1.0f};
	gameObjects[0].texture = modelTextures[0].get();
	gameObjects[0].normalMap = modelTextures[1].get();
	gameObjects[0].flags.x = 1; // disable normal map

	gameObjects[1].modelIndex = 1;
	gameObjects[1].position = {1.0f, 0.5f, 1.0f};
	gameObjects[1].scale    = {2.0f, 2.0f, 2.0f};
	gameObjects[1].texture = modelTextures[2].get();
	gameObjects[1].normalMap = modelTextures[3].get();
	gameObjects[1].flags.x = 1; // disable normal map
}

void Renderer::createPointLights() {

	pointLights.cameraPos_lightCount = glm::vec4(camera.getPosition(), MAX_POINT_LIGHTS);

	pointLights.lights[1].pos_intensity = glm::vec4(-0.2, 0.2, 0.0, defaultLightIntensity);
	pointLights.lights[1].color_attenK = glm::vec4(0.0, 0.0, 1.0, defaultLightAttenK);
}

void Renderer::createImGuiInstance() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Setup Platform/Renderer backends
	ImGui_ImplSDL3_InitForVulkan(window);
	ImGui_ImplVulkan_InitInfo init_info    = {};
	init_info.Instance                     = *instance;
	init_info.PhysicalDevice               = *physicalDevice;
	init_info.Device                       = *device;
	init_info.QueueFamily                  = graphicsFamilyIndex;
	init_info.Queue                        = *graphicsQueue;
	init_info.DescriptorPool               = *imGuiDescriptorPool;
	init_info.DescriptorPoolSize           = 100; // Setting >0 will automatically create the required pool
	init_info.MinImageCount                = MAX_FRAMES_IN_FLIGHT;
	init_info.ImageCount                   = MAX_FRAMES_IN_FLIGHT;

	init_info.UseDynamicRendering          = true;
	init_info.PipelineInfoMain.Subpass     = 0;
	init_info.PipelineInfoMain.MSAASamples = static_cast<VkSampleCountFlagBits>(msaaSamples);
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
	VkFormat imguiColorFormat = static_cast<VkFormat>(swapChainImageFormat);
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &imguiColorFormat;
	init_info.PipelineInfoMain.PipelineRenderingCreateInfo.depthAttachmentFormat = static_cast<VkFormat>(vkutil::findDepthFormat(physicalDevice));

	init_info.CheckVkResultFn              = check_vk_result;

	ImGui_ImplVulkan_Init(&init_info);
}

double Renderer::time_seconds() {
	using clock = std::chrono::steady_clock;
	static auto start = clock::now();
	return std::chrono::duration<double>(clock::now() - start).count();
}

void Renderer::initVulkan() {
	bootstrapVulkan();
	createImageViews();
	createAllocator();
	createDescriptorSetLayouts(); // used during pipeline creation, also responsible for game object descriptor set layouts

	createGeometryPipeline();
	createLightingPipeline();
	createBillboardPipeline();
	//createGraphicsPipeline();

	createCommandPool();
	createImGuiInstance();
	createGBuffer();
	createGBufferSampler();
	createColorResources();
	createDepthResources();
	createTextureSampler();

	loadModels();

	createGameObjects();
	createPointLights();

	createUniformBuffers();

	createDescriptorPool();
	createGameObjectDescriptorSets();
	createDescriptorSets();
	createCommandBuffers();

	createSyncObjects();
}

void Renderer::mainLoop() {
	bool running = true;
	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {

			ImGui_ImplSDL3_ProcessEvent(&e);

			switch (e.type) {
				case SDL_EVENT_KEY_DOWN:
					if (e.key.key == SDLK_Q) {
						running = false;
					}
					break;
				case SDL_EVENT_QUIT:
					running = false;
					break;
				case SDL_EVENT_WINDOW_RESIZED:
				case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
					framebufferResized = true;
					break;
			}
		}
		auto currentFrameTime = std::chrono::steady_clock::now();
		float deltaTime = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
		lastFrameTime = currentFrameTime;
		getKeyboardInput(deltaTime);
		drawFrame();
	}

	device.waitIdle();
}

glm::vec3 Renderer::getKeyboardInput(float deltaTime) {
	// TODO add delta time to fix frame dependent speed
	const bool* key_states = SDL_GetKeyboardState(NULL);

	auto dir_world_up = glm::vec3(0, 1, 0);
	auto dir_forward = glm::normalize(camera.getFront());
	auto dir_right = glm::normalize(glm::cross(dir_forward, dir_world_up));

	float moveSpeed = camera.getSpeed() * deltaTime;
	float lookSpeed = camera.getLookSpeed() * deltaTime;

	auto dir_vector = glm::vec3(0.0f, 0.0f, 0.0f);
	if (key_states[SDL_SCANCODE_W]) { dir_vector += dir_forward; }
	if (key_states[SDL_SCANCODE_S]) { dir_vector -= dir_forward; }
	if (key_states[SDL_SCANCODE_A]) { dir_vector -= dir_right; }
	if (key_states[SDL_SCANCODE_D]) { dir_vector += dir_right; }

	if (key_states[SDL_SCANCODE_SPACE]) { dir_vector += dir_world_up; }
	if (key_states[SDL_SCANCODE_LCTRL]) { dir_vector -= dir_world_up; }

	float deltaPitch = 0.0f, deltaYaw = 0.0f;
	if (key_states[SDL_SCANCODE_UP])	{ deltaPitch += lookSpeed; }
	if (key_states[SDL_SCANCODE_DOWN])	{ deltaPitch -= lookSpeed; }
	if (key_states[SDL_SCANCODE_LEFT])	{ deltaYaw -= lookSpeed; }
	if (key_states[SDL_SCANCODE_RIGHT]) { deltaYaw += lookSpeed; }
	camera.rotate(deltaPitch, deltaYaw);

	if (glm::length(dir_vector) > 0.0f) { dir_vector = glm::normalize(dir_vector) * moveSpeed; }

	camera.moveCamera(dir_vector);
	return dir_vector;
}

void Renderer::dumpAllocationStats() {
	char* stats = nullptr;
	vmaBuildStatsString(allocator, &stats, VK_TRUE);
	std::cerr << stats << std::endl;
	vmaFreeStatsString(allocator, stats);
}

void Renderer::cleanup() {

	device.waitIdle();

	// 1) Destroy descriptor sets first
	for (auto& obj : gameObjects) {
		obj.descriptorSets.clear();
	}

	globalDescriptorSets.clear();

	gBufferDescriptorSets.clear();

	// 2) Then destroy pools/allocator
	descriptorSetAllocator.reset();

	SDL_DestroyWindow(window);
	SDL_Quit();

	for (auto& buffer : globalUniformBuffers) {
		vkutil::destroyBuffer(allocator, buffer.buffer);
	}

	for (auto& buffer : lightingUniformBuffers) {
		vkutil::destroyBuffer(allocator, buffer.buffer);
	}

	vkutil::destroyBuffer(allocator, vertexBuffer);
	vkutil::destroyBuffer(allocator, indexBuffer);

	ktxVulkanTexture_Destruct(&ktxVkTexture, *device, nullptr);
	vkutil::destroyImage(allocator, depthImage);
	vkutil::destroyImage(allocator, colorImage);
	vkutil::destroyImage(allocator, gBuffer.albedoColorImage);
	vkutil::destroyImage(allocator, gBuffer.normalVectorImage);
	vkutil::destroyImage(allocator, gBuffer.fragPosImage);

	//dumpAllocationStats();
	destroyAllocator();

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void Renderer::drawDebugMenu() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Debug Menu");

	ImGui::ColorEdit3("Light 1 Color", glm::value_ptr(pointLights.lights[0].color_attenK));
	ImGui::ColorEdit3("Light 2 Color", glm::value_ptr(pointLights.lights[1].color_attenK));

	ImGui::DragFloat3("Light 1 Position", glm::value_ptr(pointLights.lights[0].pos_intensity), 0.0025f);
	ImGui::DragFloat3("Light 2 Position", glm::value_ptr(pointLights.lights[1].pos_intensity), 0.0025f);

	ImGui::DragFloat("Light 1 Intensity", &pointLights.lights[0].pos_intensity[3], 0.0025f);
	ImGui::DragFloat("Light 2 Intensity", &pointLights.lights[1].pos_intensity[3], 0.0025f);

	for (size_t i = 0; i < gameObjects.size(); i++) {
		if (ImGui::CollapsingHeader(("Object " + std::to_string(i)).c_str())) {
			ImGui::PushID(i);
			ImGui::DragFloat3("Rotation", glm::value_ptr(gameObjects[i].rotation), 0.005f);
			ImGui::PopID();
		}
	}

	int current = static_cast<int>(currentDebugState);
	ImGui::RadioButton("Lit", &current, 0); ImGui::SameLine();
	ImGui::RadioButton("Albedo", &current, 1); ImGui::SameLine();
	ImGui::RadioButton("Normal", &current, 2); ImGui::SameLine();
	ImGui::RadioButton("FragPos", &current, 3);
	currentDebugState = static_cast<DebugState>(current);

	ImGui::End();
	ImGui::Render();
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
		recreateGBuffer();
		return;
	}

	if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
		throw std::runtime_error("failed to acquire swap chain image");
	}

	device.resetFences(*inFlightFences[currentFrame]); // this is only performed after we handle the return values of .acquireNextImageKHR()!

	drawDebugMenu();

	commandBuffers[currentFrame].reset();

	//recordCommandBuffer(imageIndex); // forward rendering pipeline
	recordCommandBufferDeferred(imageIndex);

	vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);

	updateUniformBuffer(currentFrame);

	vk::SubmitInfo submitInfo;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &*presentCompleteSemaphores[currentFrame];
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
		recreateGBuffer();
	} else if (result != vk::Result::eSuccess) {
		throw std::runtime_error("failed to present swap chain image");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

