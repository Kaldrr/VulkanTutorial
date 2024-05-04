#pragma once

#include <vulkan/vulkan.hpp>

#include <optional>
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

	void InitializeLogicalDevice(std::span<const char* const> deviceExtensions);

	[[nodiscard]] constexpr vk::Instance GetVulkanInstance() const noexcept
	{
		return m_VulkanInstance;
	}

	[[nodiscard]] constexpr vk::DebugUtilsMessengerEXT GetDebugMessenger()
		const noexcept
	{
		return m_DebugMessenger;
	}

	[[nodiscard]] constexpr vk::Device GetLogicalDevice() const noexcept
	{
		return m_LogicalDevice;
	}

	[[nodiscard]] constexpr vk::Queue GetWorkQueue() const noexcept
	{
		return m_WorkQueue;
	}

private:
	vk::DynamicLoader m_DynamicLoader;
	vk::Instance m_VulkanInstance;

	vk::DebugUtilsMessengerEXT m_DebugMessenger;
	vk::PhysicalDevice m_PhyiscalDevice;
	vk::Device m_LogicalDevice;
	vk::Queue m_WorkQueue;
};
