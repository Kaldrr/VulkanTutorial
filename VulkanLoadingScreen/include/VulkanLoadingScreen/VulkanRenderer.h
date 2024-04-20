#pragma once

#include <QVulkanWindowRenderer>

#include <VulkanLoadingScreen/ModelManager.h>

#include <array>

#include <vulkan/vulkan.hpp>

class [[nodiscard]] VulkanRenderer final : public QVulkanWindowRenderer
{
public:
	explicit VulkanRenderer(QVulkanWindow* window, bool msaa = false);
	VulkanRenderer(const VulkanRenderer&)                = delete;
	VulkanRenderer(VulkanRenderer&&) noexcept            = delete;
	VulkanRenderer& operator=(const VulkanRenderer&)     = delete;
	VulkanRenderer& operator=(VulkanRenderer&&) noexcept = delete;
	~VulkanRenderer() noexcept final                     = default;

	void initResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
	void releaseResources() override;
	void startNextFrame() override;

private:
	[[nodiscard]] vk::ShaderModule CreateShader(const QString& name) const;
	void CreateDescriptorSetLayout();
	void CreateUniformBuffers();
	void UpdateUniformBuffer(int idx, QSize currentSize);
	void CreateDescriptorPool();
	void CreateDescriptorSets();
	void LoadTextures();
	void CreateTextureImageView();
	void CreateTextureSampler();

private:
	template <typename T>
	using FrameArray = std::array<T, QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT>;

	QVulkanWindow* const m_Window{ nullptr };
	// The value is constant for the entire lifetime of the
	// QVulkanWindow, we can make it const
	const std::uint32_t m_ConcurrentFrameCount;
	int m_SwapChainImageCount{};

	vk::Device m_Device;
	vk::PhysicalDevice m_PhysicalDevice;
	vk::RenderPass m_RenderPass;
	vk::PipelineLayout m_PipelineLayout;
	vk::Pipeline m_GraphicsPipeline;
	FrameArray<vk::Framebuffer> m_Framebuffers{};

	vk::DescriptorSetLayout m_DescriptorSetLayout;
	FrameArray<vk::Buffer> m_UniformBuffers{};
	FrameArray<vk::DeviceMemory> m_UniformDeviceMemory{};
	FrameArray<void*> m_UniformBuffersMappedMemory{};

	vk::DescriptorPool m_DescriptorPool;
	FrameArray<vk::DescriptorSet> m_DescriptorSets{};

	vk::Image m_TextureImage;
	vk::DeviceMemory m_TextureImageMemory;
	vk::ImageView m_TextureImageView;
	vk::Sampler m_TextureSampler;

	vk::Image m_DepthImage;
	vk::DeviceMemory m_DepthImageMemory;
	vk::ImageView m_DepthImageView;

	ModelManager m_ModelManager;
	std::vector<Model> m_Models;
};
