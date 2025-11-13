#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

constexpr uint32_t WIDTH = 800;
constexpr uint32_t HEIGHT = 600;

const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;
	vk::raii::Context context; // creates the RAII Vulkan_hpp context for the entire project
	vk::raii::Instance instance = nullptr;
	vk::raii::PhysicalDevice physicalDevice = nullptr;
	vk::raii::Device device = nullptr;


	void initWindow() {
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // we're not using OpenGL
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // dealing with relizable windows will come later

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void createInstance() {
		vk::ApplicationInfo appInfo{};
		appInfo.setPApplicationName("Hello Triangle")
			   .setApplicationVersion(VK_MAKE_VERSION(1,0,0))
			   .setPEngineName("No Engine")
			   .setEngineVersion(VK_MAKE_VERSION(1,0,0))
			   .setApiVersion(vk::ApiVersion14);

		// Get the required layers
		std::vector<const char*> requiredLayers;
		if (enableValidationLayers) {
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		// check if the required layers are supported by the vulkan implementation
		auto layerProperties = context.enumerateInstanceLayerProperties();
		if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const& requiredLayer) {
		return std::ranges::none_of(layerProperties,
								   [requiredLayer](auto const& layerProperty)
								   { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
		}))
		{
			throw std::runtime_error("One or more required layers are not supported!");
		}

		// Get the required instance extensions from GLFW
		uint32_t glfwExtensionCount = 0;
		auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		// Check if the required GLFW extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
			if (std::ranges::none_of(extensionProperties,
									[glfwExtension = glfwExtensions[i]](auto const& extensionProperty)
									{ return strcmp(extensionProperty.extensionName, glfwExtension) == 0; })) {

				throw std::runtime_error{"Required GLFW extension not supported"};
			}
		}

		vk::InstanceCreateInfo createInfo{};
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount  = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;
		createInfo.ppEnabledLayerNames = requiredLayers.data();
		createInfo.enabledExtensionCount = 0;
		createInfo.ppEnabledExtensionNames = nullptr;
		
		try {
		instance = vk::raii::Instance(context, createInfo);
		} catch (const vk::SystemError& err) {
			std::cerr << "Vulkan error: " << err.what() << std::endl;
		} catch (const std::exception& err) {
			std::cerr << "Error: " << err.what() << std::endl;
		}
	}

	uint32_t findQueueFamiles(vk::raii::PhysicalDevice physicalDevice) {
		// find the index of the first queue family that support graphics
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

		// get the first index into queueFamilyProperties which supports graphics
		auto graphicsQueueFamilyProperty =
			std::find_if(queueFamilyProperties.begin(),
				queueFamilyProperties.end(),
				[](vk::QueueFamilyProperties const& qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });
		return static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
	}

	std::vector<const char*> deviceExtensions = {
		vk::KHRSwapchainExtensionName,
		vk::KHRSpirv14ExtensionName,
		vk::KHRSynchronization2ExtensionName,
		vk::KHRCreateRenderpass2ExtensionName
	};

    void pickPhysicalDevice() {
    	std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
    	const auto devIter = std::ranges::find_if(devices, [&](auto const& device) {
			auto queueFamilies = device.getQueueFamilyProperties();
			bool isSuitable = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

			const auto qfpIter = std::ranges::find_if(queueFamilies, [](vk::QueueFamilyProperties const& qfp) {
				return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
			});

			isSuitable = isSuitable && (qfpIter != queueFamilies.end());

			auto extensions = device.enumerateDeviceExtensionProperties();
			bool found = true;

			for (auto const& extension : deviceExtensions) {
				auto extensionIter = std::ranges::find_if(extensions, [extension](auto const& ext) { return strcmp(ext.extensionName, extension) == 0; });
				found = found && extensionIter != extensions.end();
			}

			isSuitable = isSuitable && found;
			if (isSuitable) { physicalDevice = device; }
			return isSuitable;
		});
    	if (devIter == devices.end()) { throw std::runtime_error("failed to find a suitable GPU!"); }
	}


	void createLogicalDevice() {
	    //TODO https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/04_Logical_device_and_queues.html#_introduction
    }
	void initVulkan() {
		createInstance();
		pickPhysicalDevice();
    	createLogicalDevice();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
		}
	}

	void cleanup() {
		glfwDestroyWindow(window);
		glfwTerminate();
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