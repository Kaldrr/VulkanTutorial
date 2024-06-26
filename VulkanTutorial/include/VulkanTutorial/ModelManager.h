#pragma once
#include <cstdint>
#include <filesystem>
#include <string_view>

#include <vulkan/vulkan.hpp>

namespace Qt3DRender
{
class QMesh;
} // namespace Qt3DRender

struct Model
{
	std::string ModelName;

	std::uint32_t VertexCount{};
	vk::Buffer VertexBuffer;
	vk::DeviceMemory VertexBufferMemory;

	std::uint32_t IndexCount{};
	vk::Buffer IndexBuffer;
	vk::DeviceMemory IndexBufferMemory;
};

class [[nodiscard]] ModelManager
{
public:
	ModelManager();
	ModelManager(const ModelManager&)            = delete;
	ModelManager(ModelManager&&) noexcept        = delete;
	ModelManager& operator=(const ModelManager&) = delete;
	ModelManager& operator=(ModelManager&&)      = delete;
	~ModelManager() noexcept;

	void SetResouces(vk::Device device,
	                 vk::PhysicalDevice physicalDevice,
	                 vk::CommandPool commandPool,
	                 vk::Queue workQueue);

	void LoadModel(std::string_view modelName,
	               const std::filesystem::path& modelPath);
	void RenderAllModels(vk::CommandBuffer commandBuffer) const;
	void UnloadAllModels();

private:
	vk::Device m_Device;
	vk::PhysicalDevice m_PhysicalDevice;
	vk::CommandPool m_CommandPool;
	vk::Queue m_WorkQueue;

	std::vector<Model> m_LoadedModels;
};
