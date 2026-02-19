#include "Renderer.hpp"
#include "Config.hpp"
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void Renderer::createDescriptorSetLayout() {

	std::array bindings = {
		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, nullptr),
		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr)
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());
	descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
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
	ubo.model = rotate(glm::mat4(1.0f), sin(time * glm::radians(90.0f) * 0.5f) * 0.8f, glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(
		glm::radians(45.0f),
		static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
		0.1f, 10.0f);
	ubo.proj[1][1] *= -1;

	memcpy(uniformBuffers[imageIndex].mapped, &ubo, sizeof(ubo));
}

void Renderer::createDescriptorPool() {
	std::array poolSize {
		vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
		vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)
	};
	vk::DescriptorPoolCreateInfo poolInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, MAX_FRAMES_IN_FLIGHT, poolSize.size(), poolSize.data());

	descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void Renderer::createDescriptorSets() {
	std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);

	vk::DescriptorSetAllocateInfo allocInfo;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	descriptorSets.clear();
	descriptorSets = device.allocateDescriptorSets(allocInfo);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

		vk::DescriptorBufferInfo bufferInfo(uniformBuffers[i].buffer.buffer,0,sizeof(UniformBufferObject));
		vk::DescriptorImageInfo imageInfo(textureSampler, textureImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

		std::array descriptorWrites{
			vk::WriteDescriptorSet(descriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr ,&bufferInfo),
			vk::WriteDescriptorSet(descriptorSets[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr)
		};

		device.updateDescriptorSets(descriptorWrites, {});
	}
}