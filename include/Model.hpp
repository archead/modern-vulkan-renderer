#pragma once
#include "Types.hpp"

class Model{
private:
    std::vector<Vertex> vertices_m;
    std::vector<uint32_t> indices_m;
    AllocatedBuffer vertexBuffer_m;
    AllocatedBuffer indexBuffer_m;
    VmaAllocator* allocator_m = nullptr;

    void loadModel(std::string modelPath);

    void createVertexBuffer(vk::raii::Device &device, vk::raii::CommandPool &commandPool, vk::raii::Queue &graphicsQueue);
    void createIndexBuffer(vk::raii::Device &device, vk::raii::CommandPool &commandPool, vk::raii::Queue &graphicsQueue);

    void cleanup();

 public:

    vk::Buffer getVertexBuffer();
    vk::Buffer getIndexBuffer();
    uint32_t getIndexCount();

    Model(vk::raii::Device &device, vk::raii::CommandPool &commandPool, vk::raii::Queue &graphicsQueue, VmaAllocator* allocator, std::string modelPath);

    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    Model(Model&& other) noexcept;
    Model& operator=(Model&& other) noexcept;
 };