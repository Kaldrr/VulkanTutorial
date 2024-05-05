#pragma once

#include <vulkan/vulkan.hpp>

#include <span>

class [[nodiscard]] VulkanInstance
{
public:
	VulkanInstance(std::span<const char* const> vulkanLayers,
				   std::span<const char* const> vulkanExtensions);
	~VulkanInstance() noexcept;

	VulkanInstance(const VulkanInstance&)                = delete;
	VulkanInstance(VulkanInstance&&) noexcept            = delete;
	VulkanInstance& operator=(const VulkanInstance&)     = delete;
	VulkanInstance& operator=(VulkanInstance&&) noexcept = delete;

	void InitializeDebugMessenger();

	void InitializeDevice(std::span<const char* const> deviceExtensions,
						  vk::SurfaceKHR vulkanSurface);

	[[nodiscard]] constexpr vk::Instance GetVulkanInstance() const noexcept
	{
		return m_VulkanInstance;
	}

	[[nodiscard]] constexpr vk::DebugUtilsMessengerEXT GetDebugMessenger()
		const noexcept
	{
		return m_DebugMessenger;
	}

	[[nodiscard]] constexpr vk::PhysicalDevice GetPhysicalDevice() const noexcept
	{
		return m_PhyiscalDevice;
	}

	[[nodiscard]] constexpr vk::Device GetDevice() const noexcept
	{
		return m_Device;
	}

	[[nodiscard]] constexpr vk::Queue GetWorkQueue() const noexcept
	{
		return m_WorkQueue;
	}

	[[nodiscard]] constexpr vk::CommandPool GetCommandPool() const noexcept {
		return m_CommandPool;
	}

private:
	vk::DynamicLoader m_DynamicLoader;
	vk::Instance m_VulkanInstance;

	vk::DebugUtilsMessengerEXT m_DebugMessenger;
	vk::PhysicalDevice m_PhyiscalDevice;
	vk::Device m_Device;
	vk::Queue m_WorkQueue;
	vk::CommandPool m_CommandPool;
};
