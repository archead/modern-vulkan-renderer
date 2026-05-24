#include "Renderer.hpp"

void Renderer::createAllocator() {
    VmaAllocatorCreateInfo info{};
    info.instance = *instance;
    info.physicalDevice = *physicalDevice;
    info.device = *device;
    info.vulkanApiVersion = VK_API_VERSION_1_3;

    vmaCreateAllocator(&info, &allocator);
}

void Renderer::destroyAllocator() {
    vmaDestroyAllocator(allocator);
    allocator = nullptr;
}

