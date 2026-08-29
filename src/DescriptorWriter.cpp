#include "DescriptorWriter.hpp"

DescriptorWriter& DescriptorWriter::writeBuffer(uint32_t binding, vk::Buffer buffer, size_t offset, size_t range, vk::DescriptorType descriptorType) {
    bufferInfos.emplace_back(buffer, offset, range);
    descriptorWrites.emplace_back(VK_NULL_HANDLE, binding, 0, 1, descriptorType, nullptr, &bufferInfos.back());
    return *this;
}

DescriptorWriter& DescriptorWriter::writeImage(uint32_t binding, vk::Sampler sampler, vk::ImageView imageView, vk::ImageLayout imageLayout, vk::DescriptorType descriptorType) {
    imageInfos.emplace_back(sampler, imageView, imageLayout);
    descriptorWrites.emplace_back(VK_NULL_HANDLE, binding, 0, 1, descriptorType, &imageInfos.back(), nullptr);
    return *this;
}

void DescriptorWriter::updateSet(vk::Device device, vk::DescriptorSet descriptorSet) {
    for (auto &write : descriptorWrites) {
        write.dstSet = descriptorSet;
    }
    device.updateDescriptorSets(descriptorWrites, {});
}

