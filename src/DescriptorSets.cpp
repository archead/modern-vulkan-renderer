#include "DescriptorSets.hpp"
#include "DescriptorWriter.hpp"
#include "Renderer.hpp"
#include "Config.hpp"
#include "VkUtil.hpp"

// set layout config
void Renderer::createDescriptorSetLayouts() {

    DescriptorSetLayoutBuilder{}// per frame ubos
    .addBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)// view and proj matrices
    .addBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)// light ubo
    .build(device, globalSetLayout );

    DescriptorSetLayoutBuilder{}// per object ubo + texture sampler
    .addBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)// texture sampler
    .addBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)// normal map sampler
    .build(device, objectSetLayout);

    DescriptorSetLayoutBuilder{}// Lighting Pipeline
    .addBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)// G-buffer
    .addBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
    .addBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment)
    .addBinding(3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment)// Lighting UBO
    .build(device, gBufferSetLayout );

    DescriptorSetLayoutBuilder{}// Billboard Pipeline
    .addBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)
    .addBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex)
    .build(device, billboardSetLayout );
}

// per-game object descriptors
void Renderer::createGameObjectDescriptorSets() {
    for (auto &gameObject: gameObjects) {
        // Create descriptor sets for each FIF
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *objectSetLayout);
        gameObject.descriptorSets = descriptorSetAllocator->Allocate(layouts);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vk::ImageView normalImageView = gameObject.normalMap ? gameObject.normalMap->imageView : gameObject.texture->imageView;

            DescriptorWriter{}
            .writeImage(0, *textureSampler, gameObject.texture->imageView, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
            .writeImage(1, *textureSampler, normalImageView, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
            .updateSet(device, gameObject.descriptorSets[i]);
        }
    }
}

// per-frame descriptors
void Renderer::createDescriptorSets() {
    std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *globalSetLayout);
    globalDescriptorSets = descriptorSetAllocator->Allocate(layouts);

    std::vector<vk::DescriptorSetLayout> layouts2(MAX_FRAMES_IN_FLIGHT, *gBufferSetLayout);
    gBufferDescriptorSets = descriptorSetAllocator->Allocate(layouts2);

    std::vector<vk::DescriptorSetLayout> layouts3(MAX_FRAMES_IN_FLIGHT, *billboardSetLayout);
    billboardDescriptorSets = descriptorSetAllocator->Allocate(layouts3);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        DescriptorWriter{}
        .writeBuffer(0, globalUniformBuffers[i].buffer.buffer, 0, sizeof(GlobalUBO), vk::DescriptorType::eUniformBuffer)
        .writeBuffer(1, lightingUniformBuffers[i].buffer.buffer, 0, sizeof(LightingUBO), vk::DescriptorType::eUniformBuffer)
        .updateSet(device, globalDescriptorSets[i]);

        // configure GBuffer info and writes
        DescriptorWriter{}
        .writeImage(0, *gBufferSampler, gBuffer.fragPosImageView, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
        .writeImage(1, *gBufferSampler, gBuffer.normalVectorImageView, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
        .writeImage(2, *gBufferSampler, gBuffer.albedoColorImageView, vk::ImageLayout::eShaderReadOnlyOptimal, vk::DescriptorType::eCombinedImageSampler)
        .writeBuffer(3, lightingUniformBuffers[i].buffer.buffer, 0, sizeof(LightingUBO), vk::DescriptorType::eUniformBuffer)
        .updateSet(device, gBufferDescriptorSets[i]);

        DescriptorWriter{}
        .writeBuffer(0, globalUniformBuffers[i].buffer.buffer, 0, sizeof(GlobalUBO), vk::DescriptorType::eUniformBuffer)
        .writeBuffer(1, lightingUniformBuffers[i].buffer.buffer, 0, sizeof(LightingUBO), vk::DescriptorType::eUniformBuffer)
        .updateSet(device, billboardDescriptorSets[i]);
    }
}

// init helpers
void Renderer::createDescriptorPool() {
    descriptorSetAllocator = std::make_unique<DescriptorSetAllocator>(device, poolSize, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    descriptorSetAllocator->CreatePool();
}

// Descriptor Set Layout Builder
DescriptorSetLayoutBuilder& DescriptorSetLayoutBuilder::addBinding(uint32_t binding, vk::DescriptorType type, uint32_t count, vk::ShaderStageFlagBits shaderStage) {
    bindings.emplace_back(binding, type, count, shaderStage, nullptr);
    return *this;
}

void DescriptorSetLayoutBuilder::build(vk::raii::Device const &device, vk::raii::DescriptorSetLayout& layout) const {
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());
    layout = vk::raii::DescriptorSetLayout(device, layoutInfo);
}

// Descriptor Set Allocator
DescriptorSetAllocator::DescriptorSetAllocator(vk::raii::Device const &device, PoolSizes poolSize, vk::DescriptorPoolCreateFlags flags) :
m_device(device), m_poolSizes(std::move(poolSize)), m_flags(flags) {
}

void DescriptorSetAllocator::CreatePool() {
    vk::DescriptorPoolCreateInfo poolInfo(m_flags, m_poolSizes.maxSets, m_poolSizes.sizes.size(),
                                          m_poolSizes.sizes.data(), nullptr);
    vk::raii::DescriptorPool descriptorPool(m_device, poolInfo);
    descriptorPools.emplace_back(std::move(descriptorPool));
    currentPoolIndex = descriptorPools.size() - 1;
}

vk::DescriptorPool DescriptorSetAllocator::getCurrentPool() const {
    return *descriptorPools[currentPoolIndex];
}

std::vector<vk::raii::DescriptorSet> DescriptorSetAllocator::Allocate(std::vector<vk::DescriptorSetLayout> layouts) {
    if (descriptorPools.empty()) { CreatePool(); }

    vk::DescriptorSetAllocateInfo allocInfo(*descriptorPools[currentPoolIndex], layouts.size(), layouts.data(),
                                            nullptr);
    std::vector<vk::raii::DescriptorSet> sets = {};
    try {
        sets = m_device.allocateDescriptorSets(allocInfo);
    } catch (vk::SystemError const &e) {
        // can throw VK_ERROR_OUT_OF_POOL_MEMORY OR VK_ERROR_FRAGMENTED_POOL
        // retry the allocation one more time after creating a new pool
        CreatePool();
        allocInfo.descriptorPool = *descriptorPools[currentPoolIndex];
        sets                     = m_device.allocateDescriptorSets(allocInfo);
    }
    return sets;
}

std::vector<vk::raii::DescriptorSet> DescriptorSetAllocator::Allocate(vk::DescriptorSetLayout layout) {

    std::vector<vk::DescriptorSetLayout> layouts(1, layout);

    if (descriptorPools.empty()) { CreatePool(); }

    vk::DescriptorSetAllocateInfo allocInfo(*descriptorPools[currentPoolIndex], layouts.size(), layouts.data(),
                                            nullptr);
    std::vector<vk::raii::DescriptorSet> sets = {};
    try {
        sets = m_device.allocateDescriptorSets(allocInfo);
    } catch (vk::SystemError const &e) {
        // can throw VK_ERROR_OUT_OF_POOL_MEMORY OR VK_ERROR_FRAGMENTED_POOL
        // retry the allocation one more time after creating a new pool
        CreatePool();
        allocInfo.descriptorPool = *descriptorPools[currentPoolIndex];
        sets                     = m_device.allocateDescriptorSets(allocInfo);
    }
    return sets;
}
