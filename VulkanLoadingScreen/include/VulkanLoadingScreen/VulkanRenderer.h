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
	void createDescriptorSetLayout();
	void createUniformBuffers();
	void updateUniformBuffer(int idx, QSize currentSize);
	void createDescriptorPool();
	void createDescriptorSets();
	void loadTextures();
	void createTextureImageView();
	void createTextureSampler();

private:
	template <typename T>
	using FrameArray =
	    std::array<T, QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT>;

	QVulkanWindow* m_Window{ nullptr };
	// QVulkanDeviceFunctions* m_DeviceFunctions{ nullptr };

	vk::Device m_Device{};
	vk::PhysicalDevice m_PhysicalDevice{};
	vk::RenderPass m_RenderPass{};
	vk::PipelineLayout m_PipelineLayout{};
	vk::Pipeline m_GraphicsPipeline{};
	FrameArray<vk::Framebuffer> m_Framebuffers{};

	vk::Buffer m_VertexBuffer{};
	vk::DeviceMemory m_VertexBufferMemory{};

	vk::Buffer m_IndexBuffer{};
	vk::DeviceMemory m_IndexBufferMemory{};

	vk::DescriptorSetLayout m_DescriptorSetLayout{};
	FrameArray<vk::Buffer> m_UniformBuffers{};
	FrameArray<vk::DeviceMemory> m_UniformDeviceMemory{};
	FrameArray<void*> m_UniformBuffersMappedMemory{};

	vk::DescriptorPool m_DescriptorPool{};
	FrameArray<vk::DescriptorSet> m_DescriptorSets{};

	vk::Image m_TextureImage{};
	vk::DeviceMemory m_TextureImageMemory{};
	vk::ImageView m_TextureImageView{};
	vk::Sampler m_TextureSampler{};
};