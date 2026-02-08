#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <chrono>
#include <unordered_map> // Using for deduplicating OBJ vertices

#include <fstream>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <VkBootstrap.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;
const std::string MODEL_PATH = "C:\\dev\\vulkan-doc-tutorial\\models\\viking_room.obj";
const std::string TEXTURE_PATH = "C:\\dev\\vulkan-doc-tutorial\\textures\\viking_room.png";

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

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

struct Vertex {
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static vk::VertexInputBindingDescription getBindingDescription() {
		return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
		return{
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
			vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, texCoord))
		};
	}

	bool operator==(const Vertex& other) const {
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}
};

struct VertexHasher {
	size_t operator()(Vertex const& v) const noexcept {
		size_t seed = 0;

		auto combine = [&](size_t h) {
			seed ^= h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		};

		combine(std::hash<float>{}(v.pos.x));
		combine(std::hash<float>{}(v.pos.y));
		combine(std::hash<float>{}(v.pos.z));

		combine(std::hash<float>{}(v.color.x));
		combine(std::hash<float>{}(v.color.y));
		combine(std::hash<float>{}(v.color.z));

		combine(std::hash<float>{}(v.texCoord.x));
		combine(std::hash<float>{}(v.texCoord.y));

		return seed;
	}
};



struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:

	//region globalMembers
	GLFWwindow* window = nullptr;
	vk::raii::Context context; // creates the RAII Vulkan_hpp context for the entire project

	vk::raii::Instance instance = nullptr;
	vk::raii::SurfaceKHR surface = nullptr;
	vk::raii::PhysicalDevice physicalDevice = nullptr;
	vk::raii::Device device = nullptr;
	vkb::Device vkbDevice = {};

	uint32_t graphicsFamilyIndex = 0;
	uint32_t presentFamilyIndex = 0;

	vk::raii::Queue graphicsQueue = nullptr; // also responsible for the present queue (in my case they are in the same family)
	vk::raii::Queue presentQueue = nullptr;

	vk::raii::SwapchainKHR swapChain = nullptr;
	vkb::Swapchain vkbSwapchain = {};
	std::vector<vk::Image> swapChainImages;
	vk::Extent2D swapChainExtent{};

	vk::Format swapChainImageFormat = vk::Format::eUndefined;
	std::vector<vk::raii::ImageView> swapChainImageViews;

	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;

	vk::raii::PipelineLayout pipelineLayout = nullptr;
	vk::raii::Pipeline graphicsPipeline = nullptr;

	vk::raii::CommandPool commandPool = nullptr;

	std::vector<vk::raii::CommandBuffer> commandBuffers;
	std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore> renderCompleteSemaphores;
	std::vector<vk::raii::Fence> inFlightFences;

	bool framebufferResized = false;

	uint32_t currentFrame = 0;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	vk::raii::Buffer vertexBuffer = nullptr;
	vk::raii::DeviceMemory vertexBufferMemory = nullptr;

	vk::raii::Buffer indexBuffer = nullptr;
	vk::raii::DeviceMemory indexBufferMemory = nullptr;

	std::vector<vk::raii::Buffer> uniformBuffers;
	std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;
	vk::raii::DescriptorPool descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> descriptorSets;

	uint32_t mipLevels = 1;
	vk::raii::Image textureImage = nullptr; //this might need to be std::unique_ptr<>
	vk::raii::DeviceMemory textureImageMemory = nullptr;
	vk::raii::ImageView textureImageView = nullptr;
	vk::raii::Sampler textureSampler = nullptr;

	vk::raii::Image depthImage = nullptr;
	vk::raii::DeviceMemory depthImageMemory = nullptr;
	vk::raii::ImageView depthImageView = nullptr;

	vk::SampleCountFlagBits msaaSamples = vk::SampleCountFlagBits::e1;
	vk::raii::Image colorImage = nullptr;
	vk::raii::DeviceMemory colorImageMemory = nullptr;
	vk::raii::ImageView colorImageView = nullptr;

	std::vector<const char*> deviceExtensions = {
		vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName,
		vk::KHRCreateRenderpass2ExtensionName
	};

	//endregion

	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // we're not using OpenGL
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // dealing with resizable windows will come later

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

	void handleBootstrapErrors(auto obj_ret) {
		if (!obj_ret) {
			std::cerr << obj_ret.error().message() << "\n";
			for (auto& r : obj_ret.detailed_failure_reasons())
				std::cerr << "  - " << r << "\n";
			throw std::runtime_error("failed to select physical device!");
		}
	}

	void bootstrapVulkan() {
		// ---- Create Instance
		vkb::InstanceBuilder instance_builder;

		auto instance_ret = instance_builder
			.set_app_name("Hello Triangle")
			.set_engine_name("No Engine")
			.require_api_version(1,4,0)
			.enable_validation_layers(enableValidationLayers)
			.use_default_debug_messenger()
			.build();

		if (!instance_ret) {
			throw std::runtime_error("Failed to create Vulkan instance: " +
				instance_ret.error().message()
			);
		}
		vkb::Instance vkbInstance = instance_ret.value();
		instance = vk::raii::Instance(context, vkbInstance.instance);

		// ---- Create Surface

		VkSurfaceKHR _surface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0) {
			throw std::runtime_error("failed to create window surface!");
		}
		surface = vk::raii::SurfaceKHR(instance, _surface);

		// ---- Select Physical Device

		vkb::PhysicalDeviceSelector selector{vkbInstance};
		auto phys_ret = selector
		.set_surface(*surface)
		.set_minimum_version(1,3)
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
		// moved to separate function for easy swapchain recreation
		bootstrapSwapchain();
	}

	void bootstrapSwapchain() {
		auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
		swapChainExtent = chooseSwapExtent(surfaceCapabilities);

		auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount) {
			minImageCount = surfaceCapabilities.maxImageCount;
		}

		vkb::SwapchainBuilder swapchain_builder{vkbDevice};
		auto swap_ret = swapchain_builder
			.set_desired_min_image_count(minImageCount)
			.set_desired_extent(swapChainExtent.width, swapChainExtent.height)
			.build();

		handleBootstrapErrors(swap_ret);

		vkb::destroy_swapchain(vkbSwapchain); // destroying in case were are recreating the swapchain
		vkbSwapchain = swap_ret.value();

		swapChain = vk::raii::SwapchainKHR(device, vkbSwapchain.swapchain);
		swapChainImages = swapChain.getImages();
		swapChainImageFormat = static_cast<vk::Format>(vkbSwapchain.image_format);
	}

	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		return {
			std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
			std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
			};
	}

	void createImageViews() {
		swapChainImageViews.clear();

		vk::ImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.viewType = vk::ImageViewType::e2D;
		imageViewCreateInfo.format = swapChainImageFormat;
		imageViewCreateInfo.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

		for (auto image : swapChainImages) {
			imageViewCreateInfo.image = image;
			swapChainImageViews.emplace_back(device, imageViewCreateInfo );
		}
	}

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code) {
		vk::ShaderModuleCreateInfo createInfo;
		createInfo.codeSize = code.size() * sizeof(char);
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
		vk::raii::ShaderModule shaderModule(device, createInfo);
		return shaderModule;
	}

	void createGraphicsPipeline() {
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

	void createCommandPool() {
		vk::CommandPoolCreateInfo poolInfo;
		poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		poolInfo.queueFamilyIndex = graphicsFamilyIndex;
		commandPool = vk::raii::CommandPool(device, poolInfo);
	}

	void createCommandBuffers() {
		commandBuffers.clear();
		commandBuffers.reserve(MAX_FRAMES_IN_FLIGHT);

		vk::CommandBufferAllocateInfo allocInfo;
		allocInfo.commandPool = commandPool;
		allocInfo.level = vk::CommandBufferLevel::ePrimary;
		allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

		commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
	}

	void recordCommandBuffer(uint32_t imageIndex) {
		commandBuffers[currentFrame].begin({});

		// Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
		transition_image_layout(
			swapChainImages[imageIndex],
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eColorAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::ImageAspectFlagBits::eColor);

		transition_image_layout(
			*depthImage,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eDepthAttachmentOptimal,
			{},
			vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
			vk::PipelineStageFlagBits2::eTopOfPipe,
			vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
			vk::ImageAspectFlagBits::eDepth);

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo attachmentInfo = {};
		attachmentInfo.imageView = colorImageView;
		attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		attachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
		attachmentInfo.storeOp = vk::AttachmentStoreOp::eDontCare;
		attachmentInfo.clearValue = clearColor;
		attachmentInfo.resolveImageView = swapChainImageViews[imageIndex];
		attachmentInfo.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
		attachmentInfo.resolveMode = vk::ResolveModeFlagBits::eAverage;

		vk::RenderingAttachmentInfo depthAttachmentInfo = {};
		depthAttachmentInfo.imageView = depthImageView;
		depthAttachmentInfo.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal;
		depthAttachmentInfo.loadOp = vk::AttachmentLoadOp::eClear;
		depthAttachmentInfo.storeOp = vk::AttachmentStoreOp::eDontCare;
		depthAttachmentInfo.clearValue = clearDepth;

		vk::RenderingInfo renderingInfo = {};
		renderingInfo.renderArea.offset = vk::Offset2D(0, 0);
		renderingInfo.renderArea.extent = swapChainExtent;
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &attachmentInfo;
		renderingInfo.pDepthAttachment = &depthAttachmentInfo;

		commandBuffers[currentFrame].beginRendering(renderingInfo);

		commandBuffers[currentFrame].bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		commandBuffers[currentFrame].bindVertexBuffers(0, *vertexBuffer, {0});

		commandBuffers[currentFrame].bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

		// Set the dynamic states of Scissor and Viewport
		commandBuffers[currentFrame].setViewport(0, vk::Viewport(0.0f, 0.0f,
			static_cast<float>(swapChainExtent.width),
			static_cast<float>(swapChainExtent.height),
			0.0f, 1.0f));

		commandBuffers[currentFrame].setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));

		commandBuffers[currentFrame].bindDescriptorSets(
					vk::PipelineBindPoint::eGraphics,
					pipelineLayout,
					0,
					*descriptorSets[currentFrame],
					nullptr
					);

		commandBuffers[currentFrame].drawIndexed(indices.size(), 1, 0, 0, 0);

		commandBuffers[currentFrame].endRendering();

		transition_image_layout(
			swapChainImages[imageIndex],
			vk::ImageLayout::eColorAttachmentOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			vk::AccessFlagBits2::eColorAttachmentWrite,
			{},
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::PipelineStageFlagBits2::eBottomOfPipe,
			vk::ImageAspectFlagBits::eColor);

		commandBuffers[currentFrame].end();
	}

	void transition_image_layout(
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

	void createSyncObjects() {
		presentCompleteSemaphores.clear();
		renderCompleteSemaphores.clear();
		inFlightFences.clear();

		vk::FenceCreateInfo fenceInfo;
		fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			renderCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			inFlightFences.emplace_back(device, fenceInfo);
		}

	}


	void recreateSwapChain() {

		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		device.waitIdle();

		bootstrapSwapchain();
		createImageViews();
		createColorResources();
		createDepthResources();
	}

	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
		vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) &&
				(memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		throw std::runtime_error("failed to find suitable memory type!");
	}

	void createVertexBuffer() {
		vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		vk::raii::Buffer stagingBuffer = nullptr;
		vk::raii::DeviceMemory stagingBufferMemory = nullptr;

		createBuffer(
			bufferSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			stagingBuffer,
			stagingBufferMemory);

		void* data = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(data, vertices.data(), bufferSize);
		stagingBufferMemory.unmapMemory();

		createBuffer(
			bufferSize,
			vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			vertexBuffer,
			vertexBufferMemory);

		copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
	}

	void createIndexBuffer(){
		vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		vk::raii::Buffer stagingBuffer = nullptr;
		vk::raii::DeviceMemory stagingBufferMemory = nullptr;
		createBuffer(
			bufferSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			stagingBuffer,
			stagingBufferMemory);

		void* data = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(data, indices.data(), (size_t) bufferSize);
		stagingBufferMemory.unmapMemory();

		createBuffer(
			bufferSize,
			vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			indexBuffer,
			indexBufferMemory);

		copyBuffer(stagingBuffer, indexBuffer, bufferSize);
	}

	void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size) {
		vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
		commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
		endSingleTimeCommands(commandCopyBuffer);
	}

	void createBuffer(
		vk::DeviceSize size,
		vk::BufferUsageFlags usage,
		vk::MemoryPropertyFlags properties,
		vk::raii::Buffer& buffer,
		vk::raii::DeviceMemory& bufferMemory) {

		vk::BufferCreateInfo bufferInfo;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = vk::SharingMode::eExclusive;
		buffer = vk::raii::Buffer(device, bufferInfo);

		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

		vk::MemoryAllocateInfo allocInfo;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
		bufferMemory = vk::raii::DeviceMemory(device, allocInfo);

		buffer.bindMemory(*bufferMemory, 0);
	}

	void createImage(
		uint32_t width,
		uint32_t height,
		uint32_t mipLevels,
		vk::SampleCountFlagBits numSamples,
		vk::Format format,
		vk::ImageTiling tiling,
		vk::ImageUsageFlags usage,
		vk::MemoryPropertyFlags properties,
		vk::raii::Image& image,
		vk::raii::DeviceMemory& imageMemory) {

		vk::ImageCreateInfo imageInfo = {};
		imageInfo.imageType = vk::ImageType::e2D;
		imageInfo.format = format;
		imageInfo.extent = vk::Extent3D{width, height, 1};
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = numSamples;
		imageInfo.tiling = tiling;
		imageInfo.usage = usage;
		imageInfo.sharingMode = vk::SharingMode::eExclusive;

		image = vk::raii::Image(device, imageInfo);

		vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo = {};
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		imageMemory = vk::raii::DeviceMemory(device, allocInfo);
		image.bindMemory(*imageMemory, 0);
	}

	void createDescriptorSetLayout() {

		std::array bindings = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
		};

		vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());
		descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
	}

	void createUniformBuffers() {
		uniformBuffers.clear();
		uniformBuffersMemory.clear();
		uniformBuffersMapped.clear();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

			vk::raii::Buffer buffer({});
			vk::raii::DeviceMemory bufferMem({});

			createBuffer(
				bufferSize,
				vk::BufferUsageFlagBits::eUniformBuffer,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
				buffer,
				bufferMem
				);

			uniformBuffers.emplace_back(std::move(buffer));
			uniformBuffersMemory.emplace_back(std::move(bufferMem));
			uniformBuffersMapped.emplace_back(uniformBuffersMemory[i].mapMemory(0, bufferSize));
		}
	}

	void createDescriptorPool() {
		std::array poolSize {
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
		};
		vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, MAX_FRAMES_IN_FLIGHT, poolSize.size(), poolSize.data());

		descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
	}

	void createDescriptorSets() {
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);

		vk::DescriptorSetAllocateInfo allocInfo;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
		allocInfo.pSetLayouts = layouts.data();

		descriptorSets.clear();
		descriptorSets = device.allocateDescriptorSets(allocInfo);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

			vk::DescriptorBufferInfo bufferInfo(uniformBuffers[i],0,sizeof(UniformBufferObject));
			vk::DescriptorImageInfo imageInfo(textureSampler, textureImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

			std::array descriptorWrites{
				vk::WriteDescriptorSet(descriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr ,&bufferInfo),
				vk::WriteDescriptorSet(descriptorSets[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr)
			};

			device.updateDescriptorSets(descriptorWrites, {});
		}
	}

	void createTextureImage() {
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

		vk::DeviceSize imageSize = texWidth * texHeight * 4;

		if (!pixels) {
			throw std::runtime_error("failed to load texture image!");
		}

		vk::raii::Buffer stagingBuffer({});
		vk::raii::DeviceMemory stagingBufferMemory({});

		createBuffer(
			imageSize,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
			stagingBuffer,
			stagingBufferMemory
			);

		void* data = stagingBufferMemory.mapMemory(0, imageSize);
		memcpy(data,  pixels, imageSize);
		stagingBufferMemory.unmapMemory();

		stbi_image_free(pixels);

		createImage(texWidth,
			texHeight,
			mipLevels,
			vk::SampleCountFlagBits::e1,
			vk::Format::eR8G8B8A8Srgb,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			textureImage,
			textureImageMemory);

		transitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, mipLevels);
		copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		generateMipmaps(textureImage,vk::Format::eR8G8B8A8Srgb, texWidth, texHeight, mipLevels);
	}

	void transitionImageLayout(const vk::raii::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels) {
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

	void copyBufferToImage(const vk::raii::Buffer& buffer, vk::raii::Image& image, uint32_t width, uint32_t height) {
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

	vk::raii::CommandBuffer beginSingleTimeCommands() {
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

	void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer) {
		commandBuffer.end();

		vk::SubmitInfo submitInfo = {};
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &*commandBuffer;

		graphicsQueue.submit(submitInfo);
		graphicsQueue.waitIdle();
	}

	vk::raii::ImageView createImageView(vk::raii::Image& image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) {
		vk::ImageViewCreateInfo viewInfo{};
		viewInfo.image = image;
		viewInfo.viewType = vk::ImageViewType::e2D;
		viewInfo.format = format;
		viewInfo.subresourceRange = {aspectFlags, 0, mipLevels, 0, 1};

		return vk::raii::ImageView(device, viewInfo);
	}

	void createTextureImageView() {
		textureImageView = createImageView(textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, mipLevels);
	}

	void createTextureSampler() {
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

	void createDepthResources() {
		vk::Format depthFormat = findDepthFormat();

		createImage(
			swapChainExtent.width,
			swapChainExtent.height,
			1,
			msaaSamples,
			depthFormat,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eDepthStencilAttachment,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			depthImage,
			depthImageMemory);
		
		depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
	}

	vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
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

	vk::Format findDepthFormat() {
		return findSupportedFormat(
		  {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
		  vk::ImageTiling::eOptimal,
		  vk::FormatFeatureFlagBits::eDepthStencilAttachment
		);
	}

	bool hasStencilComponent(vk::Format format) {
		return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
	}

	void loadModel() {
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
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

	void generateMipmaps(vk::raii::Image& image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {

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

	vk::SampleCountFlagBits getMaxUsableSampleCount() {
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

	void createColorResources() {
		vk::Format colorFormat = swapChainImageFormat;

		createImage(
			swapChainExtent.width,
			swapChainExtent.height,
			1,
			msaaSamples,
			colorFormat,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
			vk::MemoryPropertyFlagBits::eDeviceLocal,
			colorImage,
			colorImageMemory);

		colorImageView = createImageView(colorImage, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
	}

	void initVulkan() {
		bootstrapVulkan();
		createImageViews();
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

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			drawFrame();
		}

		device.waitIdle();
	}

	void cleanup() {
		vkb::destroy_swapchain(vkbSwapchain);
		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void drawFrame() {
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
			throw std::runtime_error("failed ot acquire swap chain image");
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
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &*commandBuffers[currentFrame];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo. pSignalSemaphores = &*renderCompleteSemaphores[currentFrame];

		graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);

		const vk::PresentInfoKHR presentInfoKHR( *renderCompleteSemaphores[currentFrame], *swapChain, imageIndex);

		result = presentQueue.presentKHR(presentInfoKHR);

		if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
		} else if (result != vk::Result::eSuccess) {
			throw std::runtime_error("failed to present swap chain image");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void updateUniformBuffer(uint32_t imageIndex) {
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo = {};
		ubo.model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(
			glm::radians(45.0f),
			static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
			0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		memcpy(uniformBuffersMapped[imageIndex], &ubo, sizeof(ubo));
	}
};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

