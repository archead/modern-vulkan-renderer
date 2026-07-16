#include "Renderer.hpp"
#include "VkUtil.hpp"

void Renderer::createVertexBuffer() {
		vk::DeviceSize bufferSize = sizeof(Vertex) * vertices.size();

		AllocatedBuffer stagingBuffer = {};
		vkutil::createBuffer(allocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, true);

		void* data = nullptr;
		vmaMapMemory(allocator, stagingBuffer.allocation, &data);
		std::memcpy(data, vertices.data(), bufferSize);
		vmaUnmapMemory(allocator, stagingBuffer.allocation);

		vmaFlushAllocation(allocator, stagingBuffer.allocation, 0, bufferSize);

		vkutil::createBuffer(allocator, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, vertexBuffer,false);

		vkutil::copyBuffer(stagingBuffer.buffer, vertexBuffer.buffer, bufferSize, device, commandPool, graphicsQueue);
		vkutil::destroyBuffer(allocator, stagingBuffer);
	}

void Renderer::createIndexBuffer(){
		vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		AllocatedBuffer stagingBuffer = {};

		vkutil::createBuffer(allocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, true);

		void* data = nullptr;
		vmaMapMemory(allocator, stagingBuffer.allocation, &data);
		std::memcpy(data, indices.data(), bufferSize);
		vmaUnmapMemory(allocator, stagingBuffer.allocation);

		vkutil::createBuffer(allocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, false);

		vkutil::copyBuffer(stagingBuffer.buffer, indexBuffer.buffer, bufferSize, device, commandPool, graphicsQueue);
		vkutil::destroyBuffer(allocator, stagingBuffer);
	}