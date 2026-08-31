#include <vector>
#include <deque>
#include <vulkan/vulkan_raii.hpp>

class DescriptorWriter {
private:
    std::deque<vk::DescriptorBufferInfo> bufferInfos;
    std::deque<vk::DescriptorImageInfo> imageInfos;
    std::vector<vk::WriteDescriptorSet> descriptorWrites;

public:
    DescriptorWriter& writeBuffer(uint32_t binding, vk::Buffer buffer, size_t offset, size_t range, vk::DescriptorType descriptorType);
    DescriptorWriter& writeImage(uint32_t binding, vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout, vk::DescriptorType descriptorType);
    void updateSet(vk::Device device, vk::DescriptorSet descriptorSet);

    };
