#include "Model.hpp"

#include <iostream>
#include <tiny_gltf.h>
#include "MikkUtil.hpp"
#include "VkUtil.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>a
#include <numeric>

#include "glm/gtc/type_ptr.hpp"

void Model::traverseNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4 parentTransform) {
	const tinygltf::Node& node = model.nodes[nodeIndex];
	DrawItem item{};
	glm::mat4 parent = parentTransform;

	if (!node.matrix.empty()) {
		glm::mat4 transform = glm::mat4(glm::make_mat4(node.matrix.data()));
		parent = parent * transform;
	}

	else {
		// get translation
		if (!node.translation.empty()) { parent = glm::translate(parent, glm::vec3(node.translation[0],node.translation[1],node.translation[2] )); }
		// get rotation
		if (!node.rotation.empty()) {
			glm::quat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
			glm::mat4 rotation = glm::mat4_cast(q);
			parent = parent * rotation;
		}
		// get scale
		if (!node.scale.empty()) { parent = glm::scale(parent, glm::vec3(node.scale[0], node.scale[1], node.scale[2])); }
	}
	// if there's an actual mesh associated with this node > then add it to the drawList_m
	if (node.mesh > -1) {
		item.meshIndex = node.mesh;
		item.transformMatrix = parent;
		drawList_m.push_back(item);
	}
	// recursively go through the rest of the children
	for (auto& child : node.children) {
		traverseNode(model, child, parent);
	}
}

void Model::loadScene(const tinygltf::Model &model) {
	if (model.scenes.empty()) { std::cout << "No scenes found in " << modelPath_m << "\n"; return; }

	int defaultScene = model.defaultScene > -1 ? model.defaultScene : 0;
	const tinygltf::Scene& scene = model.scenes[defaultScene];

	for (auto& node : scene.nodes) {
		traverseNode(model, node, glm::mat4(1.0f));
	}
}

int Model::getAccessorIndex(tinygltf::Primitive primitive, std::string attribute) {
	auto accessorIndex = primitive.attributes.find(attribute);
	return accessorIndex == primitive.attributes.end() ? -1 : accessorIndex->second; // find return a Pair type pointer, second member being the value
}

 void Model::loadModel2(VulkanContext vkCtx, std::string modelPath) {
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err, warn;

	std::cout << "Loading Model: " << modelPath << std::endl;
	bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, modelPath);

	if (!warn.empty())	{ std::cout << "glTF warning: " << warn << std::endl; }
	if (!err.empty())	{ std::cout << "glTF error: " << err << std::endl; }
	if (!ret)			{ throw std::runtime_error("failed to load glTF model"); }

	// Load drawlist
	loadScene(model);

	// Load textures
	std::filesystem::path gltfDir = std::filesystem::path(modelPath).parent_path();
	for (auto& tex : model.textures) { // TODO HANDLE NO TEXTURE WITH A DEFAULT MATERIAL
		const auto& image = model.images[tex.source];
		std::filesystem::path imagePath = gltfDir / std::filesystem::path(image.uri).replace_extension(".ktx2");

		if (!std::filesystem::exists(imagePath)) {
			throw std::runtime_error("Cooked texture does not exist: " + imagePath.string());
		}

		textures_m.push_back(vkutil::loadTextureKTX(vkCtx, imagePath.string().c_str()));
		texturePaths_m.push_back(imagePath);
	}

	// Load materials
	for (const auto& material : model.materials) { // TODO HANDLE DEFAULT MATERIAL
		Material2 m{};
		m.baseColorTex = material.pbrMetallicRoughness.baseColorTexture.index;
		m.normalTex = material.normalTexture.index;
		m.metalRoughTex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
		m.roughnessFactor = material.pbrMetallicRoughness.roughnessFactor;
		m.metallicFactor = material.pbrMetallicRoughness.metallicFactor;
		m.normalScale = material.normalTexture.scale;

		materials_m.push_back(m);
	}

	// Load vertices

	for (const auto& mesh : model.meshes) {
		Mesh m{};

		for (const auto& primitive : mesh.primitives) {

			int tangentAccessorIndex = -1; // needed for MikkTSpace generation check outside the scope

			// Create staging buffers for verts and idxs in case tangent generation will be required
			std::vector<Vertex> vertices_l;
			std::vector<uint32_t> indices_l;

			unsigned char* posOffset = nullptr;
			size_t posStride = 0;

			unsigned char* normalOffset = nullptr;
			size_t normalStride = 0;

			unsigned char* tangentOffset = nullptr;
			size_t tangentStride = 0;

			unsigned char* texCoordOffset = nullptr;
			size_t texCoordStride = 0;

			int normalAccessorIndex = getAccessorIndex(primitive, "NORMAL");
			if (normalAccessorIndex > -1) {
				const tinygltf::Accessor& normalAccessor = model.accessors[normalAccessorIndex];
				const tinygltf::BufferView& normalBufferView = model.bufferViews[normalAccessor.bufferView];
				tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];
				normalOffset = normalBuffer.data.data() + normalAccessor.byteOffset + normalBufferView.byteOffset;
				normalStride = normalAccessor.ByteStride(normalBufferView);
			}

			tangentAccessorIndex = getAccessorIndex(primitive, "TANGENT");
			if (tangentAccessorIndex > -1) {
				const tinygltf::Accessor& tangentAccessor = model.accessors[tangentAccessorIndex];
				const tinygltf::BufferView& tangentBufferView = model.bufferViews[tangentAccessor.bufferView];
				tinygltf::Buffer& tangentBuffer = model.buffers[tangentBufferView.buffer];
				tangentOffset = tangentBuffer.data.data() + tangentAccessor.byteOffset + tangentBufferView.byteOffset;
				tangentStride = tangentAccessor.ByteStride(tangentBufferView);
			}

			int texCoordAccessorIndex = getAccessorIndex(primitive, "TEXCOORD_0");
			if (texCoordAccessorIndex > -1) {
				const tinygltf::Accessor& texCoordAccessor = model.accessors[texCoordAccessorIndex];
				const tinygltf::BufferView& texCoordBufferView = model.bufferViews[texCoordAccessor.bufferView];
				tinygltf::Buffer& texCoordBuffer= model.buffers[texCoordBufferView.buffer];
				texCoordOffset = texCoordBuffer.data.data() + texCoordAccessor.byteOffset + texCoordBufferView.byteOffset;
				texCoordStride = texCoordAccessor.ByteStride(texCoordBufferView);
			}

			int posAccessorIndex = getAccessorIndex(primitive, "POSITION");
			if (posAccessorIndex == -1) { continue; }
			const tinygltf::Accessor& posAccessor = model.accessors[posAccessorIndex];
			const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
			tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];
			posOffset = posBuffer.data.data() + posAccessor.byteOffset + posBufferView.byteOffset;
			posStride = posAccessor.ByteStride(posBufferView);


			for (size_t i = 0; i < posAccessor.count; i++) {
				Vertex v{};

				auto* pos = reinterpret_cast<const float*>(posOffset + i * posStride);
				v.pos = {pos[0], pos[1], pos[2]};

				v.color = glm::vec4{1.0f};

				if (normalOffset != nullptr) {
					auto* normal = reinterpret_cast<const float*>(normalOffset + i * normalStride);
					v.normal = {normal[0], normal[1], normal[2]};
				} else { v.normal = {0.0f, 1.0f, 0.0f}; }

				if (tangentOffset != nullptr) {
					auto* tangent = reinterpret_cast<const float*>(tangentOffset + i * tangentStride);
					v.tangent = {tangent[0], tangent[1], tangent[2], tangent[3]};
				} else { v.tangent = {0.0f, 0.0f, 0.0f, 1.0f}; }

				if (texCoordOffset != nullptr) {
					auto* texCoord = reinterpret_cast<const float*>(texCoordOffset + i * texCoordStride); // TODO HANDLE OTHER TYPES RATHER THAN REINTERPRETING
					v.texCoord = {texCoord[0], texCoord[1]};
				} else { v.texCoord = {0.0f, 0.0f}; }

				vertices_l.push_back(v);
			}

			if (primitive.indices > -1) {
				size_t                     indexStride     = 0;
				unsigned char*             indexOffset     = nullptr;
				const tinygltf::Accessor&  indexAccessor   = model.accessors[primitive.indices];
				const tinygltf::BufferView&indexBufferView = model.bufferViews[indexAccessor.bufferView];
				tinygltf::Buffer&          indexBuffer     = model.buffers[indexBufferView.buffer];
				indexOffset                                = indexBuffer.data.data() + indexAccessor.byteOffset + indexBufferView.byteOffset;
				indexStride                                = indexAccessor.ByteStride(indexBufferView);


				for (size_t i = 0; i < indexAccessor.count; i++) {
					switch (indexAccessor.componentType) {
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
							indices_l.push_back(*reinterpret_cast<uint32_t*>(indexOffset + i * indexStride)); break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
							indices_l.push_back(*reinterpret_cast<uint16_t*>(indexOffset + i * indexStride)); break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
							indices_l.push_back(*reinterpret_cast<uint8_t*>(indexOffset + i * indexStride)); break;
						default:
							throw std::runtime_error("unsupported index component type!");
					}
				}
			} else {
				indices_l.resize(posAccessor.count);
				std::iota(indices_l.begin(), indices_l.end(), 0u);
			}


			if (tangentAccessorIndex == -1 && texCoordAccessorIndex > -1) { // generate tangent if none are present in the model AND texture coords exist
				mikkutil::unweldVertices(vertices_l, indices_l);
				mikkutil::generateTangents(vertices_l, indices_l);
			}

			Primitive p{};
			p.materialIndex = primitive.material;
			p.firstIndex = indices_m.size(); // snapshot the offsets right before inserting the staging buffers
			p.vertexOffset = vertices_m.size();
			p.indexCount = indices_l.size();

			vertices_m.insert(vertices_m.end(), vertices_l.begin(), vertices_l.end());
			indices_m.insert(indices_m.end(), indices_l.begin(), indices_l.end());

			m.primitives.push_back(p);
		}
		meshes_m.push_back(m);
	}
 }

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
	vkutil::createBuffer(allocator_m, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, true);

	void* data = nullptr;
	vmaMapMemory(allocator_m, stagingBuffer.allocation, &data);
	std::memcpy(data, vertices_m.data(), bufferSize);
	vmaUnmapMemory(allocator_m, stagingBuffer.allocation);

	vmaFlushAllocation(allocator_m, stagingBuffer.allocation, 0, bufferSize);

	vkutil::createBuffer(allocator_m, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, vertexBuffer_m,false);

	vkutil::copyBuffer(stagingBuffer.buffer, vertexBuffer_m.buffer, bufferSize, device, commandPool, graphicsQueue);
	vkutil::destroyBuffer(allocator_m, stagingBuffer);
}

void Model::createIndexBuffer(vk::raii::Device &device, vk::raii::CommandPool &commandPool, vk::raii::Queue &graphicsQueue){
	vk::DeviceSize bufferSize = sizeof(indices_m[0]) * indices_m.size();

	AllocatedBuffer stagingBuffer = {};

	vkutil::createBuffer(allocator_m, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, true);
	void* data = nullptr;
	vmaMapMemory(allocator_m, stagingBuffer.allocation, &data);
	std::memcpy(data, indices_m.data(), bufferSize);
	vmaUnmapMemory(allocator_m, stagingBuffer.allocation);

	vkutil::createBuffer(allocator_m, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer_m, false);

	vkutil::copyBuffer(stagingBuffer.buffer, indexBuffer_m.buffer, bufferSize, device, commandPool, graphicsQueue);
	vkutil::destroyBuffer(allocator_m, stagingBuffer);
}

Model::Model(VulkanContext& vkCtx, std::string modelPath) {
	this->allocator_m = vkCtx.allocator;
	modelPath_m = modelPath;
	loadModel2(vkCtx, modelPath);
	createVertexBuffer(vkCtx.device, vkCtx.commandPool, vkCtx.graphicsQueue);
	createIndexBuffer(vkCtx.device, vkCtx.commandPool, vkCtx.graphicsQueue);
}

void Model::cleanup() {
	if (allocator_m != nullptr){
		if (indexBuffer_m.buffer != nullptr){ vkutil::destroyBuffer(allocator_m, indexBuffer_m);}
		if (vertexBuffer_m.buffer != nullptr){ vkutil::destroyBuffer(allocator_m, vertexBuffer_m);}
		allocator_m = nullptr;
	}
}

Model::~Model() {
	cleanup();
}

Model::Model(Model&& other) noexcept {
	indexBuffer_m    = std::move(other.indexBuffer_m);
	vertexBuffer_m   = std::move(other.vertexBuffer_m);
	indices_m        = std::move(other.indices_m);
	vertices_m       = std::move(other.vertices_m);
	allocator_m      = other.allocator_m;
	meshes_m         = std::move(other.meshes_m);
	nodes_m          = std::move(other.nodes_m);
	rootNodes_m      = std::move(other.rootNodes_m);
	materials_m      = std::move(other.materials_m);
	textures_m       = std::move(other.textures_m);
	texturePaths_m   = std::move(other.texturePaths_m);
	descriptorSets_m = std::move(other.descriptorSets_m);

	other.indexBuffer_m.buffer  = nullptr;
	other.vertexBuffer_m.buffer = nullptr;
	other.allocator_m           = nullptr;
}

Model& Model::operator=(Model&& other) noexcept {
	if (this != &other) {
		cleanup();
		indexBuffer_m    = std::move(other.indexBuffer_m);
		vertexBuffer_m   = std::move(other.vertexBuffer_m);
		indices_m        = std::move(other.indices_m);
		vertices_m       = std::move(other.vertices_m);
		allocator_m      = other.allocator_m;
		meshes_m         = std::move(other.meshes_m);
		nodes_m          = std::move(other.nodes_m);
		rootNodes_m      = std::move(other.rootNodes_m);
		materials_m      = std::move(other.materials_m);
		textures_m       = std::move(other.textures_m);
		texturePaths_m   = std::move(other.texturePaths_m);
		descriptorSets_m = std::move(other.descriptorSets_m);

		other.indexBuffer_m.buffer  = nullptr;
		other.vertexBuffer_m.buffer = nullptr;
		other.allocator_m           = nullptr;
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



