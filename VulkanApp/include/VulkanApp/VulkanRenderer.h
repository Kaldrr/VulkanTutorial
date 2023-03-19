#pragma once

#include <QVulkanWindowRenderer>

#include <array>

class VulkanRenderer : public QVulkanWindowRenderer
{
public:
	explicit VulkanRenderer(QVulkanWindow* window, bool msaa = false);

	void initResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
	void releaseResources() override;
	void startNextFrame() override;

protected:
	VkShaderModule createShader(const QString& name);

	QVulkanWindow* m_Window{ nullptr };
	QVulkanDeviceFunctions* m_DeviceFunctions{ nullptr };

	VkDeviceMemory m_BufferMemory{ VK_NULL_HANDLE };
	VkBuffer m_Buffer{ VK_NULL_HANDLE };
	std::array<VkDescriptorBufferInfo, QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT> m_UniformBufferInfo{};

	VkDescriptorPool m_DescriptorPool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout m_DescriptorSetLayout{ VK_NULL_HANDLE };
	std::array<VkDescriptorSet, QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT> m_DescriptorSet{};

	VkPipelineCache m_PipelineCache{ VK_NULL_HANDLE };
	VkPipelineLayout m_PipelineLayout{ VK_NULL_HANDLE };
	VkPipeline m_Pipeline{ VK_NULL_HANDLE };

	QMatrix4x4 m_Projection{};
	std::array<float,3> m_Rotations{};
};