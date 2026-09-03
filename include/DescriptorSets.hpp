#pragma once
#include <vulkan/vulkan_raii.hpp>
#include <chrono>

struct PoolSizes {
	uint32_t maxSets = 256;
	std::vector<vk::DescriptorPoolSize> sizes = {
		{vk::DescriptorType::eUniformBuffer, 256},
		{vk::DescriptorType::eCombinedImageSampler, 256},
		{vk::DescriptorType::eSampledImage, 256},
		{vk::DescriptorType::eSampler, 256},
	};
};

class DescriptorSetLayoutBuilder {
public:
    DescriptorSetLayoutBuilder& addBinding(uint32_t binding, vk::DescriptorType type, uint32_t count, vk::ShaderStageFlagBits shaderStage);
    void build(vk::raii::Device const& device, vk::raii::DescriptorSetLayout& layout) const;
private:
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
};

class DescriptorSetAllocator {
public:
    DescriptorSetAllocator(vk::raii::Device const& device, PoolSizes poolSize, vk::DescriptorPoolCreateFlags flags);
    void CreatePool();
    [[nodiscard]] std::vector<vk::raii::DescriptorSet> Allocate(std::vector<vk::DescriptorSetLayout> layouts);
    [[nodiscard]] std::vector<vk::raii::DescriptorSet> Allocate(vk::DescriptorSetLayout layout);
    vk::DescriptorPool getCurrentPool() const;

private:
    vk::raii::Device const& m_device = nullptr;
    PoolSizes m_poolSizes;
    vk::DescriptorPoolCreateFlags m_flags;
    std::vector<vk::raii::DescriptorPool> descriptorPools;
    size_t currentPoolIndex = 0;
};