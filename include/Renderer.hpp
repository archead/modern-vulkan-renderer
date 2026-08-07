#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL.h>
#include <VkBootstrap.h>
#include <memory>

#include "Types.hpp"
#include "DescriptorSets.hpp"
#include "Config.hpp"
#include <ktxvulkan.h>

#include "Camera.hpp"
#include "Model.hpp"

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

	uint32_t        graphicsFamilyIndex = 0;
	uint32_t        presentFamilyIndex  = 0;
	vk::raii::Queue graphicsQueue       = nullptr;
	vk::raii::Queue presentQueue        = nullptr;
	// also responsible for the present queue (in my case they are in the same family)

	vk::raii::SwapchainKHR           swapChain = nullptr;
	vkb::Swapchain                   vkbSwapchain{};
	std::vector<vk::Image>           swapChainImages;
	vk::Extent2D                     swapChainExtent{};
	vk::Format                       swapChainImageFormat = vk::Format::eUndefined;
	std::vector<vk::raii::ImageView> swapChainImageViews;

	vk::raii::PipelineLayout pipelineLayout         = nullptr;
	vk::raii::Pipeline       graphicsPipeline       = nullptr;

	vk::raii::Pipeline       geometryPipeline       = nullptr;
	vk::raii::PipelineLayout geometryPipelineLayout = nullptr;

	vk::raii::Pipeline       lightingPipeline       = nullptr;
	vk::raii::PipelineLayout lightingPipelineLayout = nullptr;

	vk::raii::CommandPool                commandPool = nullptr;
	std::vector<vk::raii::CommandBuffer> commandBuffers;
	std::vector<vk::raii::Semaphore>     presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore>     renderCompleteSemaphores;
	std::vector<vk::raii::Fence>         inFlightFences;

	bool framebufferResized = false;

	uint32_t currentFrame = 0;


	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;

	VmaAllocator    allocator    = {};
	AllocatedBuffer vertexBuffer = {};
	AllocatedBuffer indexBuffer  = {};

	std::vector<Model> models;

	// set 0 (global)
	std::vector<AllocatedUniformBuffer>		globalUniformBuffers;
	std::vector<AllocatedUniformBuffer>		lightingUniformBuffers;
	vk::raii::DescriptorSetLayout           globalSetLayout = nullptr;
	std::vector<vk::raii::DescriptorSet>    globalDescriptorSets;
	// set 1 (object) NOTE: the sets and buffers are declared per gameObject
	vk::raii::DescriptorSetLayout			objectSetLayout = nullptr;
	// set 2 (deferred lighting), non-FIF
	vk::raii::DescriptorSetLayout gBufferSetLayout = nullptr;
	std::vector<vk::raii::DescriptorSet> gBufferDescriptorSets;

	PoolSizes                               poolSize{};
	DescriptorSetLayoutBuilder              layoutBuilder{};
	std::unique_ptr<DescriptorSetAllocator> descriptorSetAllocator;
	vk::raii::DescriptorPool                imGuiDescriptorPool = nullptr;

	ktxVulkanTexture    ktxVkTexture     = {};
	uint32_t            mipLevels        = 1;
	AllocatedImage      textureImage     = {};
	vk::Format          textureFormat    = vk::Format::eUndefined;
	vk::raii::ImageView textureImageView = nullptr;
	vk::raii::Sampler   textureSampler   = nullptr;

	AllocatedImage          depthImage     = {};
	vk::raii::ImageView     depthImageView = nullptr;
	vk::SampleCountFlagBits msaaSamples    = vk::SampleCountFlagBits::e1;
	AllocatedImage          colorImage     = {};
	vk::raii::ImageView     colorImageView = nullptr;

	struct GBuffer {
		vk::Format fragPosFormat = vk::Format::eR16G16B16A16Sfloat;
		vk::Format albedoColorFormat = vk::Format::eR8G8B8A8Unorm;
		vk::Format normalVectorFormat = vk::Format::eR16G16B16A16Sfloat;

		AllocatedImage fragPosImage = {};
		AllocatedImage albedoColorImage = {};
		AllocatedImage normalVectorImage = {};

		vk::raii::ImageView fragPosImageView = nullptr;
		vk::raii::ImageView albedoColorImageView = nullptr;
		vk::raii::ImageView normalVectorImageView = nullptr;
	};

	GBuffer gBuffer = {};
	vk::raii::Sampler gBufferSampler = nullptr;

	std::vector<const char *> deviceExtensions = { vk::KHRSwapchainExtensionName };

	struct ModelTexture {
		ktxVulkanTexture             ktxVkTexture = {};
		const VkAllocationCallbacks *allocator    = nullptr;
		VkDevice                     device       = VK_NULL_HANDLE;
		vk::Sampler                  sampler      = nullptr; // this is a reference to global sampler
		vk::raii::ImageView          imageView    = nullptr;

		~ModelTexture() {
			if (device && ktxVkTexture.image != VK_NULL_HANDLE) {
				ktxVulkanTexture_Destruct(&ktxVkTexture, device, allocator);
				ktxVkTexture = {};
			}
		}

		// remove copy and assign operators / constructors
		ModelTexture() = default;
		ModelTexture(const ModelTexture&) = delete;
		ModelTexture& operator=(const ModelTexture&) = delete;
	};

	struct GameObject {
		glm::vec3 position = {0.0f, 0.0f, 0.0f};
		glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
		glm::vec3 scale    = {1.0f, 1.0f, 1.0f};

		size_t							 modelIndex;
		ModelTexture *                       texture   = nullptr;
		ModelTexture *                       normalMap = nullptr;
		glm::uvec4                           flags     = {0, 0, 0, 0};
		std::vector<AllocatedUniformBuffer>  uniformBuffers;
		std::vector<vk::raii::DescriptorSet> descriptorSets;

		// Calculate model matrix based on position, rotation and scale
		[[nodiscard]] glm::mat4 getModelMatrix() const;
	};

	std::vector<std::unique_ptr<ModelTexture>> modelTextures;

	std::vector<GameObject> gameObjects;

	Camera camera;

	glm::vec3 defaultLightColor    = {0.96, 0.89, 0.54};
	float     defaultLightAttenK   = 0.25f;
	glm::vec3 defaultLightPos      = {0.5, 0.2, 0.4};
	float     defaultLightIntensity = 0.75;

	struct Light {
		glm::vec4 pos_intensity = {0.5, 0.5, 0.5, 0.75}; // using vec4 to align the data xyz = pos, w = intensity
		glm::vec4 color_attenK = {1.0, 1.0, 1.0, 0.25};
	};

	struct LightingUBO {
		Light     light[MAX_POINT_LIGHTS];
		glm::vec4 cameraPos_lightCount = {0.0f, 0.0f, 0.0f, MAX_POINT_LIGHTS}; // xyz = cameraPos, w = lightCount

	};

	LightingUBO pointLights = {};

	DebugState currentDebugState = DebugState::Lit;

	//endregion

	void handleBootstrapErrors(auto obj_ret);

	void bootstrapVulkan();

	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

	void createImageViews();

	void createLightingPipeline();

	void createGeometryPipeline();

	void createGraphicsPipeline();

	void createCommandPool();

	void createCommandBuffers();

	void recordCommandBufferDeferred(uint32_t imageIndex);

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

	std::unique_ptr<Renderer::ModelTexture> loadTextureKTX(const char *texturePath);

	void createVertexBuffer();

	void createIndexBuffer();

	void createGameObjectDescriptorSets();

	void createDescriptorSetLayouts();

	void createGameObjectUniformBuffers();

	void createUniformBuffers();

	void updateGameObjectUniformBuffer(uint32_t imageIndex);

	void createDescriptorPool();

	void createDescriptorSets();

	void createTextureImage();

	void createTextureImageView();

	void createTextureSampler();

	void createDepthResources();

	bool hasStencilComponent(vk::Format format);

	void loadModelGLTF(std::string modelPath);

	void createColorResources();

	void createGBuffer();

	void recreateGBuffer();

	void createGBufferSampler();

	void createAllocator();

	void destroyAllocator();

	void createGameObjects();

	void createPointLights();

	void createImGuiInstance();

	double time_seconds();

	void initVulkan();

	void mainLoop();

	glm::vec3 getKeyboardInput();

	void dumpAllocationStats();

	void cleanup();

	void drawDebugMenu();

	void drawFrame();

	void updateUniformBuffer(uint32_t imageIndex);

	void loadModels();
};
