#pragma once

#include <vulkan/vulkan.hpp>

[[nodiscard]] vk::RenderPass createRenderPass(
    const vk::Device& device,
    const VkFormat colorFormat,
    const VkFormat depthFormat,
    const std::uint32_t sampleCount);

[[nodiscard]] std::tuple<vk::PipelineColorBlendStateCreateInfo,
                         vk::PipelineLayout>
createPipelineLayoutInfo(
    const vk::Device& device,
    const vk::DescriptorSetLayout& descriptorSetLayout);

[[nodiscard]] std::uint32_t findMemoryType(
    const vk::PhysicalDevice& physicalDevice,
    const vk::MemoryPropertyFlags memoryProperties,
    const std::uint32_t typeFilter);

[[nodiscard]] std::tuple<vk::Buffer, vk::DeviceMemory>
createDeviceBuffer(const vk::DeviceSize bufferSize,
                   const vk::BufferUsageFlags bufferFlags,
                   const vk::MemoryPropertyFlags memoryFlags,
                   const vk::Device& device,
                   const vk::PhysicalDevice& physicalDevie);

void copyBuffer(const vk::Buffer& dstBuffer,
                const vk::Buffer& srcBuffer,
                const vk::DeviceSize size,
                const vk::CommandPool& commandPool,
                const vk::Device& device,
                const vk::Queue& graphicsQueue);

[[nodiscard]] vk::CommandBuffer beginSingleTimeCommands(
    const vk::Device& device,
    const vk::CommandPool& commandPool);
void endSingleTimeCommands(const vk::CommandBuffer& commandBuffer,
                           const vk::Queue& queue);

void transitionImageLayout(const vk::Image& image,
                           const vk::Format format,
                           const vk::ImageLayout oldLayout,
                           const vk::ImageLayout newLayout,
                           const vk::Device& device,
                           const vk::CommandPool& commandPool,
                           const vk::Queue& workQueue);

void copyBufferToImage(const vk::Buffer& buffer,
                       const vk::Image& image,
                       uint32_t width,
                       uint32_t height,
                       const vk::Device& device,
                       const vk::CommandPool& commandPool,
                       const vk::Queue& queue);