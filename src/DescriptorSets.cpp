#include "DescriptorSets.hpp"
#include "Renderer.hpp"
#include "Config.hpp"
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>

// per-game object descriptors
void Renderer::createGameObjectDescriptorSets() {
	for (auto& gameObject : gameObjects) {
		// Create descriptor sets for each FIF
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout0);
		gameObject.descriptorSets.clear();
		gameObject.descriptorSets = descriptorSetAllocator->Allocate(layouts);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DescriptorBufferInfo bufferInfo(gameObject.uniformBuffers[i].buffer.buffer, 0, sizeof(TransformUBO));
			vk::DescriptorImageInfo imageInfo(textureSampler, textureImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

			std::array descriptorWrites{
				vk::WriteDescriptorSet(gameObject.descriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo),
				vk::WriteDescriptorSet(gameObject.descriptorSets[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr)
			};
			device.updateDescriptorSets(descriptorWrites, {});
		}
	}
}

void Renderer::createGameObjectUniformBuffers() {
	for (auto& gameObject : gameObjects) {
		gameObject.uniformBuffers.clear();
		gameObject.uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DeviceSize bufferSize = sizeof(TransformUBO);
			createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, gameObject.uniformBuffers[i].buffer, true);
			gameObject.uniformBuffers[i].mapped = gameObject.uniformBuffers[i].buffer.allocInfo.pMappedData;
		}
	}
}

void Renderer::updateGameObjectUniformBuffer(uint32_t imageIndex) {
	// Probably not needed
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	// Camera and proj matrices that are shared among all objects
	glm::mat4 view = glm::lookAt(cameraPos + cameraPosOffset, cameraCenter + cameraCenterOffset, glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 proj = glm::perspective(
	glm::radians(45.0f),
		static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
		0.1f, 50.0f
	);

	proj[1][1] *= -1;

	for (auto& gameObject : gameObjects) {
		// add some rotation to each object
		gameObject.rotation.y = time;
		glm::mat4 initialRotation = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		glm::mat4 model = gameObject.getModelMatrix() * initialRotation;

		TransformUBO ubo{ model, view, proj };

		// create normal matrix from the model matrix
		ubo.normalMatrix = glm::transpose(glm::inverse(ubo.model));

		memcpy(gameObject.uniformBuffers[imageIndex].mapped, &ubo, sizeof(ubo));
	}
}


// per-frame descriptors
void Renderer::createDescriptorSetLayouts() {
	// configure layout for the transformation matrix + sampler, this layout is used for the gameobject
	DescriptorSetLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
	builder.addBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
	descriptorSetLayout0 = builder.build(device);

	// configure layout for the lighting descriptor
	builder.clearBindings();
	builder.addBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment);
	descriptorSetLayout1 = builder.build(device);
}

void Renderer::createDescriptorSets() {
	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout0);
	descriptorSets0.clear();
	descriptorSets0 = descriptorSetAllocator->Allocate(layouts);

	layouts.clear();
	layouts = std::vector<vk::DescriptorSetLayout>(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout1);
	descriptorSets1.clear();
	descriptorSets1 = descriptorSetAllocator->Allocate(layouts);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

		vk::DescriptorBufferInfo bufferInfo(matrixAndSamplerUniformBuffers[i].buffer.buffer, 0, sizeof(TransformUBO));
		vk::DescriptorImageInfo imageInfo(textureSampler, textureImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

		std::array descriptorWrites0{
			vk::WriteDescriptorSet(descriptorSets0[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr ,&bufferInfo),
			vk::WriteDescriptorSet(descriptorSets0[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr)
		};
		device.updateDescriptorSets(descriptorWrites0, {});

		// do the same for the lighting uniform / descriptor set
		bufferInfo = vk::DescriptorBufferInfo(lightingUniformBuffers[i].buffer.buffer, 0, sizeof(LightUBO));
		std::array descriptorWrites1 = {vk::WriteDescriptorSet(descriptorSets1[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr ,&bufferInfo)};
		device.updateDescriptorSets(descriptorWrites1, {});
	}
}

void Renderer::createUniformBuffers() {
	// config transformation matrices + sampler ubo
	matrixAndSamplerUniformBuffers.clear();
	matrixAndSamplerUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vk::DeviceSize bufferSize = sizeof(TransformUBO);
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, matrixAndSamplerUniformBuffers[i].buffer, true);
		matrixAndSamplerUniformBuffers[i].mapped = matrixAndSamplerUniformBuffers[i].buffer.allocInfo.pMappedData;
	}

	// config light props ubo
	lightingUniformBuffers.clear();
	lightingUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vk::DeviceSize bufferSize = sizeof(LightUBO);
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, lightingUniformBuffers[i].buffer, true);
		lightingUniformBuffers[i].mapped = lightingUniformBuffers[i].buffer.allocInfo.pMappedData;
	}
}

void Renderer::updateUniformBuffer(uint32_t imageIndex) {
	static auto startTime = std::chrono::high_resolution_clock::now();
	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	TransformUBO ubo = {};
	ubo.model = glm::rotate(glm::mat4(1.0f), sin(time * glm::radians(90.0f) * 0.5f) * 0.8f, glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(
		glm::radians(45.0f),
		static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
		0.1f, 50.0f);
	ubo.proj[1][1] *= -1;
	ubo.normalMatrix = glm::transpose(glm::inverse(ubo.model));

	memcpy(matrixAndSamplerUniformBuffers[imageIndex].mapped, &ubo, sizeof(ubo));

	LightUBO light_ubo            = {};
	light_ubo.light.color_attenK  = glm::vec4(lightColor, lightAttenK);
	light_ubo.cameraPos           = glm::vec4(cameraPos, 0.0f);
	light_ubo.light.pos_intensity = glm::vec4(lightPos, lightIntesity);

	memcpy(lightingUniformBuffers[imageIndex].mapped, &light_ubo, sizeof(light_ubo));
}


// init helpers
void Renderer::createDescriptorPool() {
	descriptorSetAllocator = std::make_unique<DescriptorSetAllocator>(device, poolSize, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
	descriptorSetAllocator->CreatePool();
}

// Descriptor Set Layout Builder
void DescriptorSetLayoutBuilder::addBinding(uint32_t binding, vk::DescriptorType type, uint32_t count, vk::ShaderStageFlagBits shaderStage) {
	bindings.emplace_back(binding, type, count, shaderStage, nullptr);
}

void DescriptorSetLayoutBuilder::clearBindings() {
		bindings.clear();
}

vk::raii::DescriptorSetLayout DescriptorSetLayoutBuilder::build(vk::raii::Device const& device) const {
	vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());
	return {vk::raii::DescriptorSetLayout(device, layoutInfo)};
}

// Descriptor Set Allocator
DescriptorSetAllocator::DescriptorSetAllocator(vk::raii::Device const& device, PoolSizes poolSize, vk::DescriptorPoolCreateFlags flags):
	m_device(device),
	m_poolSizes(std::move(poolSize)),
	m_flags(flags) {}

void DescriptorSetAllocator::CreatePool() {
	vk::DescriptorPoolCreateInfo poolInfo(m_flags, m_poolSizes.maxSets, m_poolSizes.sizes.size(), m_poolSizes.sizes.data(), nullptr);
	vk::raii::DescriptorPool descriptorPool(m_device, poolInfo);
	descriptorPools.emplace_back(std::move(descriptorPool));
	currentPool = &descriptorPools.back();
}

vk::DescriptorPool DescriptorSetAllocator::getCurrentPool() const {
	return **currentPool;
}

std::vector<vk::raii::DescriptorSet> DescriptorSetAllocator::Allocate(std::vector<vk::DescriptorSetLayout> layouts) {
	if (!currentPool) { CreatePool(); }

	vk::DescriptorSetAllocateInfo allocInfo(**currentPool, layouts.size(), layouts.data(), nullptr);
	std::vector<vk::raii::DescriptorSet> sets = {};
	try {
		sets = m_device.allocateDescriptorSets(allocInfo);
	}
	catch (vk::SystemError const& e)  {
		// can throw VK_ERROR_OUT_OF_POOL_MEMORY OR VK_ERROR_FRAGMENTED_POOL
		// retry the allocation one more time after creating a new pool
		CreatePool();
		allocInfo.descriptorPool = **currentPool;
		sets = m_device.allocateDescriptorSets(allocInfo);

	}
	return sets;
}