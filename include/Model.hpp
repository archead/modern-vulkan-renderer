#pragma once
#include "Types.hpp"
#include <filesystem>
#include <tiny_gltf.h>

class Model{
private:
    std::vector<Vertex> vertices_m;
    std::vector<uint32_t> indices_m;
    AllocatedBuffer vertexBuffer_m;
    AllocatedBuffer indexBuffer_m;

    VmaAllocator          allocator_m = nullptr;
    std::string            modelPath_m;

    void loadModel(std::string modelPath);
    void loadModel2(VulkanContext vkCtx, std::string modelPath);
    int getAccessorIndex(tinygltf::Primitive primitive, std::string attribute);
    void createVertexBuffer(vk::raii::Device &device, vk::raii::CommandPool &commandPool, vk::raii::Queue &graphicsQueue);
    void createIndexBuffer(vk::raii::Device &device, vk::raii::CommandPool &commandPool, vk::raii::Queue &graphicsQueue);
    void traverseNode(const tinygltf::Model& model, int nodeIndex, const glm::mat4 parentTransform);
    void loadScene(const tinygltf::Model& model);
    void cleanup();

 public:

    std::vector<Mesh> meshes_m;
    std::vector<DrawItem> drawList_m;
    std::vector<Material> materials_m;
    std::vector<std::unique_ptr<ModelTexture>> textures_m;
    std::vector<std::filesystem::path> texturePaths_m;
    std::vector<vk::raii::DescriptorSet> descriptorSets_m;

    vk::Buffer getVertexBuffer();
    vk::Buffer getIndexBuffer();
    uint32_t getIndexCount();

    Model(VulkanContext& vkCtx, std::string modelPath);

    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    Model(Model&& other) noexcept;
    Model& operator=(Model&& other) noexcept;

 };