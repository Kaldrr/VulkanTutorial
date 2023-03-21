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

private:
	QVulkanWindow* m_Window{ nullptr };
	// QVulkanDeviceFunctions* m_DeviceFunctions{ nullptr };
	QSize m_PreviousWindowSize{ 0, 0 };
	
	vk::Device m_Device{};
	vk::RenderPass m_RenderPass{};
	vk::PipelineLayout m_PipelineLayout{};
	vk::Pipeline m_GraphicsPipeline{};
	std::array<vk::Framebuffer,
	           QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT>
	    m_Framebuffers{};
};