#include <UniformBuffers.hpp>
#include "VkUtil.hpp"
#include "Renderer.hpp"

void Renderer::createGameObjectUniformBuffers() {
    for (auto &gameObject: gameObjects) {
        gameObject.uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkutil::createBuffer(allocator, sizeof(ObjectUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, gameObject.uniformBuffers[i].buffer, true);
            gameObject.uniformBuffers[i].mapped = gameObject.uniformBuffers[i].buffer.allocInfo.pMappedData;
        }
    }
}

void Renderer::updateGameObjectUniformBuffer(uint32_t imageIndex) {
    for (auto &gameObject: gameObjects) {

        // apply transformations to the model matrix here (rotate, scale, etc.)
        ObjectUBO ubo    = {};
        ubo.model        = gameObject.getModelMatrix();
        ubo.normalMatrix = glm::transpose(glm::inverse(ubo.model));
        ubo.flags = gameObject.flags;

        memcpy(gameObject.uniformBuffers[imageIndex].mapped, &ubo, sizeof(ubo));
    }
}

void Renderer::createUniformBuffers() {
    globalUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkutil::createBuffer(allocator, sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, globalUniformBuffers[i].buffer, true);
        globalUniformBuffers[i].mapped = globalUniformBuffers[i].buffer.allocInfo.pMappedData;
    }

    // config light props ubo
    lightingUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkutil::createBuffer(allocator, sizeof(LightingUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, lightingUniformBuffers[i].buffer, true);
        lightingUniformBuffers[i].mapped = lightingUniformBuffers[i].buffer.allocInfo.pMappedData;
    }
}

void Renderer::updateUniformBuffer(uint32_t imageIndex) {
    GlobalUBO globalUbo = {};
    globalUbo.view      = camera.getViewMatrix();
    globalUbo.proj = camera.getProjectionMatrix(static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height));
    // map the global UBO
    memcpy(globalUniformBuffers[imageIndex].mapped, &globalUbo, sizeof(globalUbo));
    // map lighting UBO
    memcpy(lightingUniformBuffers[imageIndex].mapped, &pointLights, sizeof(pointLights));
}

