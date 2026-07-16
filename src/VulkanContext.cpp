#include "VulkanContext.hpp"
#include <iostream>

void VulkanContext::init(SDL_Window *window) {

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

void VulkanContext::handleBootstrapErrors(auto obj_ret) {
	if (!obj_ret) {
		std::cerr << obj_ret.error().message() << "\n";
		for (auto& r : obj_ret.detailed_failure_reasons())
			std::cerr << "  - " << r << "\n";
		throw std::runtime_error("failed to select physical device!");
	}
}
