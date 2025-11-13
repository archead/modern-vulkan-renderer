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
		try {
		instance = vk::raii::Instance(context, createInfo);
		} catch (const vk::SystemError& err) {
			std::cerr << "Vulkan error: " << err.what() << std::endl;
		} catch (const std::exception& err) {
			std::cerr << "Error: " << err.what() << std::endl;
		}
	}

	void initVulkan() {
		createInstance();
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