#include "DescriptorSets.hpp"
#include "Renderer.hpp"
#include "Config.hpp"
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>

void Renderer::createGameObjectDescriptorSets() {
	for (auto& gameObject : gameObjects) {
		// Create descriptor sets for each FIF
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
		gameObject.descriptorSets.clear();
		gameObject.descriptorSets = descriptorSetAllocator->Allocate(layouts);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vk::DescriptorBufferInfo bufferInfo(gameObject.uniformBuffers[i].buffer.buffer, 0, sizeof(UniformBufferObject));
			vk::DescriptorImageInfo imageInfo(textureSampler, textureImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

			std::array descriptorWrites{
				vk::WriteDescriptorSet(gameObject.descriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo),
				vk::WriteDescriptorSet(gameObject.descriptorSets[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr)
			};
			device.updateDescriptorSets(descriptorWrites, {});
		}
	}
}

void Renderer::createDescriptorSetLayout() {
	DescriptorSetLayoutBuilder builder;
	builder.addBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
	builder.addBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
	descriptorSetLayout = builder.build(device);
}

void Renderer::createUniformBuffers() {
	uniformBuffers.clear();
	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniformBuffers[i].buffer, true);
		uniformBuffers[i].mapped = uniformBuffers[i].buffer.allocInfo.pMappedData;
	}
}

void Renderer::updateUniformBuffer(uint32_t imageIndex) {
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo = {};
	ubo.model = glm::rotate(glm::mat4(1.0f), sin(time * glm::radians(90.0f) * 0.5f) * 0.8f, glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(
		glm::radians(45.0f),
		static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
		0.1f, 10.0f);
	ubo.proj[1][1] *= -1;

	memcpy(uniformBuffers[imageIndex].mapped, &ubo, sizeof(ubo));
}

void Renderer::createDescriptorPool() {
	descriptorSetAllocator = std::make_unique<DescriptorSetAllocator>(device, poolSize, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
	descriptorSetAllocator->CreatePool();
}

void Renderer::createDescriptorSets() {
	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
	descriptorSets.clear();
	descriptorSets = descriptorSetAllocator->Allocate(layouts);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

		vk::DescriptorBufferInfo bufferInfo(uniformBuffers[i].buffer.buffer, 0, sizeof(UniformBufferObject));
		vk::DescriptorImageInfo imageInfo(textureSampler, textureImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

		std::array descriptorWrites{
			vk::WriteDescriptorSet(descriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr ,&bufferInfo),
			vk::WriteDescriptorSet(descriptorSets[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr)
		};

		device.updateDescriptorSets(descriptorWrites, {});
	}
}

// Descriptor Set Layout Builder
void DescriptorSetLayoutBuilder::addBinding(uint32_t binding, vk::DescriptorType type, uint32_t count, vk::ShaderStageFlagBits shaderStage) {
	bindings.emplace_back(binding, type, count, shaderStage, nullptr);
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