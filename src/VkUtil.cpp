#include "VkUtil.hpp"

void vkutil::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size,vk::raii::Device& device, vk::raii::CommandPool& commandPool, vk::raii::Queue& queue) {
    vk::raii::CommandBuffer cmd = beginSingleTimeCommands(device, commandPool);
    cmd.copyBuffer(vk::Buffer(srcBuffer), vk::Buffer(dstBuffer), vk::BufferCopy(0, 0, size));
    endSingleTimeCommands(cmd, queue);
}

void vkutil::createBuffer(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, AllocatedBuffer &allocBuff, bool hostVisible) {

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    if (hostVisible) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    const VkResult res = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &allocBuff.buffer, &allocBuff.allocation, &allocBuff.allocInfo);

    if (res != VK_SUCCESS) {
        throw std::runtime_error("vmaCreateBuffer failed!");
    }
}

void vkutil::destroyBuffer(VmaAllocator allocator, AllocatedBuffer& allocBuff) {
    vmaDestroyBuffer(allocator, allocBuff.buffer, allocBuff.allocation);
}

vk::raii::CommandBuffer vkutil::beginSingleTimeCommands(vk::raii::Device& device, vk::raii::CommandPool& commandPool) {
    vk::CommandBufferAllocateInfo allocInfo = {};
    allocInfo.commandPool = commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());

    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void vkutil::endSingleTimeCommands(vk::raii::CommandBuffer& commandBuffer, vk::raii::Queue& graphicsQueue) {
    commandBuffer.end();

    vk::SubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*commandBuffer;

    graphicsQueue.submit(submitInfo);
    graphicsQueue.waitIdle();
}
