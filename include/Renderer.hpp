#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <VkBootstrap.h>

#include "Types.hpp"
#include "DescriptorSets.hpp"
#include "Config.hpp"

class Renderer {
public:
	void run();

private:

	//region globalMembers
	SDL_Window *      window = nullptr;
	vk::raii::Context context; // creates the RAII Vulkan_hpp context for the entire project

	vk::raii::Instance               instance       = nullptr;
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::SurfaceKHR             surface        = nullptr;
	vk::raii::PhysicalDevice         physicalDevice = nullptr;
	vk::raii::Device                 device         = nullptr;
	vkb::Device                      vkbDevice      = {};

	uint32_t graphicsFamilyIndex = 0;
	uint32_t presentFamilyIndex  = 0;

	vk::raii::Queue graphicsQueue = nullptr;
	// also responsible for the present queue (in my case they are in the same family)
	vk::raii::Queue presentQueue = nullptr;

	vk::raii::SwapchainKHR swapChain    = nullptr;
	vkb::Swapchain         vkbSwapchain{};
	std::vector<vk::Image> swapChainImages;
	vk::Extent2D           swapChainExtent{};

	vk::Format                       swapChainImageFormat = vk::Format::eUndefined;
	std::vector<vk::raii::ImageView> swapChainImageViews;

	VmaAllocator allocator = {};

	vk::raii::PipelineLayout pipelineLayout   = nullptr;
	vk::raii::Pipeline       graphicsPipeline = nullptr;

	vk::raii::CommandPool commandPool = nullptr;

	std::vector<vk::raii::CommandBuffer> commandBuffers;
	std::vector<vk::raii::Semaphore>     presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore>     renderCompleteSemaphores;
	std::vector<vk::raii::Fence>         inFlightFences;

	bool framebufferResized = false;

	uint32_t currentFrame = 0;

	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;

	AllocatedBuffer vertexBuffer = {};
	AllocatedBuffer indexBuffer  = {};

	std::vector<AllocatedUniformBuffer>     uniformBuffers = {};
	PoolSizes                               poolSize{};
	vk::raii::DescriptorSetLayout           descriptorSetLayout = nullptr;
	DescriptorSetLayoutBuilder              layoutBuilder{};
	std::unique_ptr<DescriptorSetAllocator> descriptorSetAllocator;
	std::vector<vk::raii::DescriptorSet>    descriptorSets;

	uint32_t            mipLevels        = 1;
	AllocatedImage      textureImage     = {};
	vk::raii::ImageView textureImageView = nullptr;
	vk::raii::Sampler   textureSampler   = nullptr;

	AllocatedImage      depthImage     = {};
	vk::raii::ImageView depthImageView = nullptr;

	vk::SampleCountFlagBits msaaSamples    = vk::SampleCountFlagBits::e1;
	AllocatedImage          colorImage     = {};
	vk::raii::ImageView     colorImageView = nullptr;

	std::vector<const char *> deviceExtensions = {
		vk::KHRSwapchainExtensionName
	};

	struct GameObject {
		glm::vec3 position = {0.0f, 0.0f, 0.0f};
		glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
		glm::vec3 scale = {1.0f, 1.0f, 1.0f};

		// Uniform buffer for this object (one per frame in flight)
		std::vector<AllocatedUniformBuffer> uniformBuffers;

		// Descriptor sets for this object (one per frame in flight)
		std::vector<vk::raii::DescriptorSet> descriptorSets;

		// Calculate model matrix based on position, rotation and scale
		[[nodiscard]] glm::mat4 getModelMatrix() const;
	};

	std::array<GameObject, MAX_OBJECTS> gameObjects;

	//endregion

	void handleBootstrapErrors(auto obj_ret);

	void bootstrapVulkan();

	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

	void createImageViews();

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char>& code);

	void createGraphicsPipeline();

	void createCommandPool();

	void createCommandBuffers();

	void recordCommandBuffer(uint32_t imageIndex);

	void transition_image_layout(
		vk::Image               image,
		vk::ImageLayout         oldLayout,
		vk::ImageLayout         newLayout,
		vk::AccessFlags2        srcAccessMask,
		vk::AccessFlags2        dstAccessMask,
		vk::PipelineStageFlags2 srcStageMask,
		vk::PipelineStageFlags2 dstStageMask,
		vk::ImageAspectFlags    image_aspect_flags);

	void createSyncObjects();

	void recreateSwapChain();

	void cleanupSwapchain();

	void createVertexBuffer();

	void createIndexBuffer();

	void copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size);

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	void createBuffer( VkDeviceSize size, VkBufferUsageFlags usage, AllocatedBuffer &allocBuff, bool hostVisible);

	void destroyBuffer(VmaAllocator allocator, AllocatedBuffer& allocBuff);

	void destroyImage(VmaAllocator allocator, AllocatedImage& allocImage);

	void createImage(
		uint32_t              width,
		uint32_t              height,
		uint32_t              mipLevels,
		VkSampleCountFlagBits numSamples,
		VkFormat              format,
		VkImageTiling         tiling,
		VkImageUsageFlags     usage,
		AllocatedImage &      image);

	void createGameObjectDescriptorSets();

	void createDescriptorSetLayout();

	void createGameObjectUniformBuffers();

	void createUniformBuffers();

	void createDescriptorPool();

	void createDescriptorSets();

	void createTextureImage();

	void transitionImageLayout(const vk::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, uint32_t mipLevels);

	void copyBufferToImage(const vk::Buffer& buffer, vk::Image image, uint32_t width, uint32_t height);

	void copyBufferToImage(const vk::Buffer &buffer, vk::Image image, vk::ImageLayout layout,
	                       const std::vector<vk::BufferImageCopy> &regions);

	vk::raii::CommandBuffer beginSingleTimeCommands();

	void endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer);

	vk::raii::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);

	void createTextureImageView();

	void createTextureSampler();

	void createDepthResources();

	vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

	vk::Format findDepthFormat();

	bool hasStencilComponent(vk::Format format);

	void loadModel();

	void loadModelGLTF();

	void generateMipmaps(vk::Image image, vk::Format imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

	vk::SampleCountFlagBits getMaxUsableSampleCount();

	void createColorResources();

	void createAllocator();

	void destroyAllocator();

	void setupGameObjects();

	void initVulkan();

	void mainLoop();

	void dumpAllocationStats();

	void cleanup();

	void drawFrame();

	void updateUniformBuffer(uint32_t imageIndex);
};
