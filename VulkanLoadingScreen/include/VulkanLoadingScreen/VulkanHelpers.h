#pragma once

#include <cstdint>

#include <vulkan/vulkan.hpp>

// TODO: Many reworks, and add namespace here
// Or just move it all into a class prefferably

[[nodiscard]] vk::RenderPass CreateRenderPass(vk::Device device,
                                              VkFormat colorFormat,
                                              VkFormat depthFormat,
                                              std::uint32_t sampleCount);

[[nodiscard]] std::tuple<vk::PipelineColorBlendStateCreateInfo, vk::PipelineLayout>
CreatePipelineLayoutInfo(vk::Device device,
                         vk::DescriptorSetLayout descriptorSetLayout);

[[nodiscard]] std::uint32_t FindMemoryType(vk::PhysicalDevice physicalDevice,
                                           vk::MemoryPropertyFlags memoryProperties,
                                           std::uint32_t typeFilter);

[[nodiscard]] std::tuple<vk::Buffer, vk::DeviceMemory> CreateDeviceBuffer(
    vk::DeviceSize bufferSize,
    vk::BufferUsageFlags bufferFlags,
    vk::MemoryPropertyFlags memoryFlags,
    vk::Device device,
    vk::PhysicalDevice physicalDevie);

void CopyBuffer(vk::Buffer dstBuffer,
                vk::Buffer srcBuffer,
                vk::DeviceSize size,
                vk::CommandPool commandPool,
                vk::Device device,
                vk::Queue graphicsQueue);

[[nodiscard]] vk::CommandBuffer BeginSingleTimeCommands(
    vk::Device device,
    vk::CommandPool commandPool);

void EndSingleTimeCommands(vk::CommandBuffer commandBuffer, vk::Queue queue);

void TransitionImageLayout(vk::Image image,
                           vk::Format format,
                           vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout,
                           vk::Device device,
                           vk::CommandPool commandPool,
                           vk::Queue workQueue);

void CopyBufferToImage(vk::Buffer buffer,
                       vk::Image image,
                       uint32_t width,
                       uint32_t height,
                       vk::Device device,
                       vk::CommandPool commandPool,
                       vk::Queue queue);
