#pragma once
#include <cstdint>
#include <filesystem>

#include <vulkan/vulkan.hpp>

namespace Qt3DRender
{
class QMesh;
}

struct Model
{
	std::string m_ModelName{};

	std::uint32_t m_VertexCount{};
	vk::Buffer m_VertexBuffer{};
	vk::DeviceMemory m_VertexBufferMemory{};

	std::uint32_t m_IndexCount{};
	vk::Buffer m_IndexBuffer{};
	vk::DeviceMemory m_IndexBufferMemory{};
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

	void LoadModel(std::string modelName,
				   const std::filesystem::path& modelPath);
	void RenderAllModels(vk::CommandBuffer commandBuffer);
	void UnloadAllModels();

private:
	vk::Device m_Device;
	vk::PhysicalDevice m_PhysicalDevice;
	vk::CommandPool m_CommandPool;
	vk::Queue m_WorkQueue;

	std::vector<Model> m_LoadedModels{};
};
