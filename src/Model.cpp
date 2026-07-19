#include "Model.hpp"
#include <iostream>
#include <tiny_gltf.h>
#include "MikkUtil.hpp"
#include "VkUtil.hpp"

void Model::loadModel(std::string modelPath) {
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	std::cout << "Loading Model: " << modelPath << std::endl;
	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, modelPath);

	if (!warn.empty())	{ std::cout << "glTF warning: " << warn << std::endl; }
	if (!err.empty())	{ std::cout << "glTF error: " << err << std::endl; }
	if (!ret)			{ throw std::runtime_error("failed to load glTF model"); }

	// Process all meshes in the model
	std::unordered_map<Vertex, uint32_t, VertexHasher> uniqueVertices{};

	for (const auto& mesh : model.meshes) {
		for (const auto& primitive : mesh.primitives) {
			if (primitive.indices == -1) { continue; } // doesn't handle cases where there is no index data but should prevent out of range vector reads

			// Get indices
			const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
			const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
			const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];

			// Get vertex positions
			const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
			const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
			const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

			// Get texture coordinates if available
			bool hasTexCoords = primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end();
			const tinygltf::Accessor* texCoordAccessor = nullptr;
			const tinygltf::BufferView* texCoordBufferView = nullptr;
			const tinygltf::Buffer* texCoordBuffer = nullptr;

			// Get normals if available
			bool hasNormals = primitive.attributes.find("NORMAL") != primitive.attributes.end();
			const tinygltf::Accessor* normalAccessor = nullptr;
			const tinygltf::BufferView* normalBufferView = nullptr;
			const tinygltf::Buffer* normalBuffer = nullptr;

			if (hasTexCoords) {
				texCoordAccessor = &model.accessors[primitive.attributes.at("TEXCOORD_0")];
				texCoordBufferView = &model.bufferViews[texCoordAccessor->bufferView];
				texCoordBuffer = &model.buffers[texCoordBufferView->buffer];
			}

			if (hasNormals) {
				normalAccessor = &model.accessors[primitive.attributes.at("NORMAL")];
				normalBufferView = &model.bufferViews[normalAccessor->bufferView];
				normalBuffer = &model.buffers[normalBufferView->buffer];
			}

			std::vector<uint32_t> remap(posAccessor.count); // used to deduplicate indices as well

			// Process vertices
			for (size_t i = 0; i < posAccessor.count; i++) {
				Vertex vertex{};

				// Get position
				const float* pos = reinterpret_cast<const float*>(&posBuffer.data[posBufferView.byteOffset + posAccessor.byteOffset + i * 12]); // hardcoded byte offset 12 can be improved
				vertex.pos = {pos[0], pos[1], pos[2]};

				// Get texture coordinates if available
				if (hasTexCoords) {
					const float* texCoord = reinterpret_cast<const float*>(&texCoordBuffer->data[texCoordBufferView->byteOffset + texCoordAccessor->byteOffset + i * 8]); // hardcoded byte offset 8 can be improved
					vertex.texCoord = {texCoord[0], texCoord[1]};
				} else { vertex.texCoord = {0.0f, 0.0f}; }

				// Set default color
				vertex.color = {1.0f, 1.0f, 1.0f};

				if (hasNormals) {
					const float* normal = reinterpret_cast<const float*>(&normalBuffer->data[normalBufferView->byteOffset + normalAccessor->byteOffset + i * 12]); // hardcoded byte offset 12 can be improved
					vertex.normal = {normal[0], normal[1], normal[2]};
				} else { vertex.normal = {0.0f, 0.0f, 0.0f}; }

				// Add vertex if unique
				if (!uniqueVertices.contains(vertex)) {
					remap[i] = static_cast<uint32_t>(vertices_m.size());
					uniqueVertices[vertex] = remap[i];
					vertices_m.push_back(vertex);
				} else {
					remap[i] = uniqueVertices[vertex];
				}
			}

			// Process indices
			const unsigned char* indexData = &indexBuffer.data[indexBufferView.byteOffset + indexAccessor.byteOffset];

			// Handle different index component types
			if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
				const auto* indices16 = reinterpret_cast<const uint16_t*>(indexData);
				for (size_t i = 0; i < indexAccessor.count; i++) { indices_m.push_back(remap[indices16[i]]); }
			}
			else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
				const auto* indices32 = reinterpret_cast<const uint32_t*>(indexData);
				for (size_t i = 0; i < indexAccessor.count; i++) { indices_m.push_back(remap[indices32[i]]); }
			}
			else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
				const auto* indices8 = reinterpret_cast<const uint8_t*>(indexData);
				for (size_t i = 0; i < indexAccessor.count; i++) { indices_m.push_back(remap[indices8[i]]); }
			}
		}
	}
	mikkutil::unweldVertices(vertices_m, indices_m);
	mikkutil::generateTangents(vertices_m, indices_m);
}

void Model::createVertexBuffer(vk::raii::Device &device, vk::raii::CommandPool &commandPool, vk::raii::Queue &graphicsQueue) {
	vk::DeviceSize bufferSize = sizeof(Vertex) * vertices_m.size();

	AllocatedBuffer stagingBuffer = {};
	vkutil::createBuffer(*allocator_m, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, true);

	void* data = nullptr;
	vmaMapMemory(*allocator_m, stagingBuffer.allocation, &data);
	std::memcpy(data, vertices_m.data(), bufferSize);
	vmaUnmapMemory(*allocator_m, stagingBuffer.allocation);

	vmaFlushAllocation(*allocator_m, stagingBuffer.allocation, 0, bufferSize);

	vkutil::createBuffer(*allocator_m, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, vertexBuffer_m,false);

	vkutil::copyBuffer(stagingBuffer.buffer, vertexBuffer_m.buffer, bufferSize, device, commandPool, graphicsQueue);
	vkutil::destroyBuffer(*allocator_m, stagingBuffer);
}

void Model::createIndexBuffer(vk::raii::Device &device, vk::raii::CommandPool &commandPool, vk::raii::Queue &graphicsQueue){
	vk::DeviceSize bufferSize = sizeof(indices_m[0]) * indices_m.size();

	AllocatedBuffer stagingBuffer = {};

	vkutil::createBuffer(*allocator_m, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, true);
	void* data = nullptr;
	vmaMapMemory(*allocator_m, stagingBuffer.allocation, &data);
	std::memcpy(data, indices_m.data(), bufferSize);
	vmaUnmapMemory(*allocator_m, stagingBuffer.allocation);

	vkutil::createBuffer(*allocator_m, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer_m, false);

	vkutil::copyBuffer(stagingBuffer.buffer, indexBuffer_m.buffer, bufferSize, device, commandPool, graphicsQueue);
	vkutil::destroyBuffer(*allocator_m, stagingBuffer);
}

Model::Model(vk::raii::Device &device, vk::raii::CommandPool &commandPool, vk::raii::Queue &graphicsQueue, VmaAllocator* allocator, std::string modelPath) {
	this->allocator_m = allocator;
	loadModel(modelPath);
	createVertexBuffer(device, commandPool, graphicsQueue);
	createIndexBuffer(device, commandPool, graphicsQueue);
}

void Model::cleanup() {
	if (allocator_m != nullptr){
		if (indexBuffer_m.buffer != nullptr){ vkutil::destroyBuffer(*allocator_m, indexBuffer_m);}
		if (vertexBuffer_m.buffer != nullptr){vkutil::destroyBuffer(*allocator_m, vertexBuffer_m);}
		allocator_m = nullptr;
	}
}

Model::~Model() {
	cleanup();
}

Model::Model(Model&& other) noexcept {
	indexBuffer_m = std::move(other.indexBuffer_m);
	vertexBuffer_m = std::move(other.vertexBuffer_m);
	indices_m = std::move(other.indices_m);
	vertices_m = std::move(other.vertices_m);
	allocator_m = other.allocator_m;

	other.indexBuffer_m.buffer = nullptr;
	other.vertexBuffer_m.buffer = nullptr;
	other.allocator_m = nullptr;
}

Model& Model::operator=(Model&& other) noexcept {
	if (this != &other) {
		cleanup();
		indexBuffer_m = std::move(other.indexBuffer_m);
		vertexBuffer_m = std::move(other.vertexBuffer_m);
		indices_m = std::move(other.indices_m);
		vertices_m = std::move(other.vertices_m);
		allocator_m = other.allocator_m;

		other.indexBuffer_m.buffer = nullptr;
		other.vertexBuffer_m.buffer = nullptr;
		other.allocator_m = nullptr;
	}
	return *this;
}

vk::Buffer Model::getIndexBuffer() {
	return indexBuffer_m.buffer;
}

vk::Buffer Model::getVertexBuffer() {
	return vertexBuffer_m.buffer;
}

uint32_t Model::getIndexCount() {
	return static_cast<uint32_t>(indices_m.size());
}



