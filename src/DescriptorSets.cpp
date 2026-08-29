#include "DescriptorSets.hpp"
#include "Renderer.hpp"
#include "Config.hpp"
#include "VkUtil.hpp"

// set layout config
void Renderer::createDescriptorSetLayouts() {
    // per frame ubos
    DescriptorSetLayoutBuilder builder0;
    // view and proj matrices
    builder0.addBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
    // light ubo
    builder0.addBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment);
    globalSetLayout = builder0.build(device);

    // per object ubo + texture sampler
    DescriptorSetLayoutBuilder builder1;
    // model and normal matrices
    builder1.addBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
    // texture sampler
    builder1.addBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
    // normal map sampler
    builder1.addBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
    objectSetLayout = builder1.build(device);

    // Lighting Pipeline
    DescriptorSetLayoutBuilder builder2;
    // G-buffer
    builder2.addBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
    builder2.addBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
    builder2.addBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment);
    // Lighting UBO
    builder2.addBinding(3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment);
    gBufferSetLayout = builder2.build(device);

    // Billboard Pipeline
    DescriptorSetLayoutBuilder builder3;
    builder3.addBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
    builder3.addBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex);
    billboardSetLayout = builder3.build(device);
}

// per-game object descriptors
void Renderer::createGameObjectDescriptorSets() {
    for (auto &gameObject: gameObjects) {
        // Create descriptor sets for each FIF
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *objectSetLayout);
        gameObject.descriptorSets = descriptorSetAllocator->Allocate(layouts);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vk::DescriptorBufferInfo bufferInfo(gameObject.uniformBuffers[i].buffer.buffer, 0, sizeof(ObjectUBO));
            vk::DescriptorImageInfo  imageInfo(*textureSampler, gameObject.texture->imageView, vk::ImageLayout::eShaderReadOnlyOptimal);

            vk::ImageView normalImageView = gameObject.normalMap ? gameObject.normalMap->imageView : gameObject.texture->imageView;
            vk::DescriptorImageInfo  normalMapInfo(*textureSampler, normalImageView, vk::ImageLayout::eShaderReadOnlyOptimal);

            std::array<vk::WriteDescriptorSet, 3> descriptorWrites{
                vk::WriteDescriptorSet(gameObject.descriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo),
                vk::WriteDescriptorSet(gameObject.descriptorSets[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr),
                vk::WriteDescriptorSet(gameObject.descriptorSets[i], 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &normalMapInfo, nullptr)
            };
            device.updateDescriptorSets(descriptorWrites, {});
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
        vk::DescriptorBufferInfo globalBufferInfo(globalUniformBuffers[i].buffer.buffer, 0, sizeof(GlobalUBO));
        vk::DescriptorBufferInfo lightingBufferInfo(lightingUniformBuffers[i].buffer.buffer, 0, sizeof(LightingUBO));

        std::array<vk::WriteDescriptorSet, 2> writes{
            vk::WriteDescriptorSet(globalDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &globalBufferInfo),
            vk::WriteDescriptorSet(globalDescriptorSets[i], 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &lightingBufferInfo)
        };
        device.updateDescriptorSets(writes, {});

        // configure GBuffer info and writes
        vk::DescriptorImageInfo gBufferFragPosInfo(*gBufferSampler, gBuffer.fragPosImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo gBufferNormalInfo(*gBufferSampler, gBuffer.normalVectorImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorImageInfo gBufferAlbedoInfo(*gBufferSampler, gBuffer.albedoColorImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
        vk::DescriptorBufferInfo lightingUBOInfo(lightingUniformBuffers[i].buffer.buffer, 0, sizeof(LightingUBO));

        std::array<vk::WriteDescriptorSet, 4> writes2 {
            vk::WriteDescriptorSet(gBufferDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &gBufferFragPosInfo, nullptr),
            vk::WriteDescriptorSet(gBufferDescriptorSets[i], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &gBufferNormalInfo, nullptr),
            vk::WriteDescriptorSet(gBufferDescriptorSets[i], 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &gBufferAlbedoInfo, nullptr),
            vk::WriteDescriptorSet(gBufferDescriptorSets[i], 3, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &lightingUBOInfo)
            };
        device.updateDescriptorSets(writes2, {});

        vk::DescriptorBufferInfo billboardLightingUBOInfo(lightingUniformBuffers[i].buffer.buffer, 0, sizeof(LightingUBO));
        vk::DescriptorBufferInfo billboardCameraInfo(globalUniformBuffers[i].buffer.buffer, 0, sizeof(GlobalUBO));

        std::array<vk::WriteDescriptorSet, 2> writes3 {
            vk::WriteDescriptorSet(billboardDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &billboardCameraInfo),
            vk::WriteDescriptorSet(billboardDescriptorSets[i], 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &billboardLightingUBOInfo)
        };
        device.updateDescriptorSets(writes3, {});
    }
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

vk::raii::DescriptorSetLayout DescriptorSetLayoutBuilder::build(vk::raii::Device const &device) const {
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, bindings.size(), bindings.data());
    return {vk::raii::DescriptorSetLayout(device, layoutInfo)};
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
