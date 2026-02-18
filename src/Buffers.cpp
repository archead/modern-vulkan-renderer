#include "Renderer.hpp"

void Renderer::createVertexBuffer() {
		vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		AllocatedBuffer stagingBuffer = {};
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, true);

		void* data = nullptr;
		vmaMapMemory(allocator, stagingBuffer.allocation, &data);
		std::memcpy(data, vertices.data(), bufferSize);
		vmaUnmapMemory(allocator, stagingBuffer.allocation);

		vmaFlushAllocation(allocator, stagingBuffer.allocation, 0, bufferSize);

		createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, vertexBuffer,false);

		copyBuffer(stagingBuffer.buffer, vertexBuffer.buffer, bufferSize);
		destroyBuffer(allocator, stagingBuffer);
	}

	void Renderer::createIndexBuffer(){
		vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		AllocatedBuffer stagingBuffer = {};

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, true);

		void* data = nullptr;
		vmaMapMemory(allocator, stagingBuffer.allocation, &data);
		std::memcpy(data, indices.data(), bufferSize);
		vmaUnmapMemory(allocator, stagingBuffer.allocation);

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, false);

		copyBuffer(stagingBuffer.buffer, indexBuffer.buffer, bufferSize);
		destroyBuffer(allocator, stagingBuffer);
	}

	void Renderer::copyBuffer(vk::raii::Buffer& srcBuffer, vk::raii::Buffer& dstBuffer, vk::DeviceSize size) {
		vk::raii::CommandBuffer cmd = beginSingleTimeCommands();
		cmd.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
		endSingleTimeCommands(cmd);
	}

	void Renderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		vk::raii::CommandBuffer cmd = beginSingleTimeCommands();
		cmd.copyBuffer(vk::Buffer(srcBuffer), vk::Buffer(dstBuffer), vk::BufferCopy(0, 0, size));
		endSingleTimeCommands(cmd);
	}

	void Renderer::createBuffer( VkDeviceSize size, VkBufferUsageFlags usage, AllocatedBuffer &allocBuff, bool hostVisible) {

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

	void Renderer::destroyBuffer(VmaAllocator allocator, AllocatedBuffer& allocBuff) {
		vmaDestroyBuffer(allocator, allocBuff.buffer, allocBuff.allocation);
	}
