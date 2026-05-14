#include "DescriptorSets.hpp"
#include "Renderer.hpp"
#include "Config.hpp"
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>

// per-game object descriptors
void Renderer::createGameObjectDescriptorSets() {
	for (auto& gameObject : gameObjects) {
		// Create descriptor sets for each FIF
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *objectSetLayout);
		gameObject.descriptorSets = descriptorSetAllocator->Allocate(layouts);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DescriptorBufferInfo bufferInfo(gameObject.uniformBuffers[i].buffer.buffer, 0, sizeof(ObjectUBO));
			//vk::DescriptorImageInfo imageInfo(textureSampler, textureImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

			std::array<vk::WriteDescriptorSet, 1> descriptorWrites {
				vk::WriteDescriptorSet(gameObject.descriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo),
			//	vk::WriteDescriptorSet(gameObject.descriptorSets[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr)
			};
			device.updateDescriptorSets(descriptorWrites, {});
		}
	}
}

void Renderer::createGameObjectUniformBuffers() {
	for (auto& gameObject : gameObjects) {
		gameObject.uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			createBuffer(sizeof(ObjectUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, gameObject.uniformBuffers[i].buffer, true);
			gameObject.uniformBuffers[i].mapped = gameObject.uniformBuffers[i].buffer.allocInfo.pMappedData;
		}
	}
}

void Renderer::updateGameObjectUniformBuffer(uint32_t imageIndex) {

	for (auto& gameObject : gameObjects) {
		// add some rotation to each object
		glm::mat4 model = gameObject.getModelMatrix();
		// apply transformations to the model matrix here (rotate, scale, etc.)

		ObjectUBO ubo = {};
		ubo.model = model;
		ubo.normalMatrix = glm::transpose(glm::inverse(ubo.model));

		memcpy(gameObject.uniformBuffers[imageIndex].mapped, &ubo, sizeof(ubo));
	}
}


void Renderer::createDescriptorSetLayouts() {
	// per frame ubos
	DescriptorSetLayoutBuilder builder0;
	builder0.addBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex); // view and proj matrices
	builder0.addBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment); // light ubo
	globalSetLayout = builder0.build(device);

	// per object ubo + texture sampler
	DescriptorSetLayoutBuilder builder1;
	builder1.addBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex); // model and normal matrices
//	builder1.addBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment); // texture sampler
	objectSetLayout = builder1.build(device);
}

// per-frame descriptors
void Renderer::createDescriptorSets() {
	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *globalSetLayout);
	globalDescriptorSets = descriptorSetAllocator->Allocate(layouts);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

		vk::DescriptorBufferInfo globalBufferInfo(globalUniformBuffers[i].buffer.buffer, 0, sizeof(GlobalUBO));
		vk::DescriptorBufferInfo lightingBufferInfo(lightingUniformBuffers[i].buffer.buffer, 0, sizeof(LightingUBO));

		std::array<vk::WriteDescriptorSet, 2> writes {
			vk::WriteDescriptorSet(globalDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &globalBufferInfo),
			vk::WriteDescriptorSet(globalDescriptorSets[i], 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &lightingBufferInfo)
		};
		device.updateDescriptorSets(writes, {});
	}
}

void Renderer::createUniformBuffers() {

	globalUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		createBuffer(sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,globalUniformBuffers[i].buffer, true);
		globalUniformBuffers[i].mapped = globalUniformBuffers[i].buffer.allocInfo.pMappedData;
	}

	// config light props ubo
	lightingUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		createBuffer(sizeof(LightingUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, lightingUniformBuffers[i].buffer, true);
		lightingUniformBuffers[i].mapped = lightingUniformBuffers[i].buffer.allocInfo.pMappedData;
	}
}

void Renderer::updateUniformBuffer(uint32_t imageIndex) {
	GlobalUBO ubo = {};
	ubo.view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(
		glm::radians(45.0f),
		static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
		0.1f, 50.0f);
	ubo.proj[1][1] *= -1;

	memcpy(globalUniformBuffers[imageIndex].mapped, &ubo, sizeof(ubo));

	LightingUBO light_ubo            = {};
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