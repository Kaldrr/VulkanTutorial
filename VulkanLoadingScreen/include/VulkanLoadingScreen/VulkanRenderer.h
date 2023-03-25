#pragma once

#include <QVulkanWindowRenderer>

#include <array>

#include <vulkan/vulkan.hpp>

class VulkanRenderer : public QVulkanWindowRenderer
{
public:
	explicit VulkanRenderer(QVulkanWindow* window, bool msaa = false);

	void initResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
	void releaseResources() override;
	void startNextFrame() override;

private:
	[[nodiscard]] vk::ShaderModule createShader(const QString& name);
	void createVertexBuffer();
	void createIndexBuffer();

private:
	QVulkanWindow* m_Window{ nullptr };
	// QVulkanDeviceFunctions* m_DeviceFunctions{ nullptr };
	
	vk::Device m_Device{};
	vk::RenderPass m_RenderPass{};
	vk::PipelineLayout m_PipelineLayout{};
	vk::Pipeline m_GraphicsPipeline{};
	std::array<vk::Framebuffer,
	           QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT>
	    m_Framebuffers{};

	vk::Buffer m_VertexBuffer{};
	vk::DeviceMemory m_VertexBufferMemory{};

	vk::Buffer m_IndexBuffer{};
	vk::DeviceMemory m_IndexBufferMemory{};
};