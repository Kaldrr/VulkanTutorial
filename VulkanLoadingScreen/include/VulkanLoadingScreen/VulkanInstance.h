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

	void InitializeLogicalDevice(std::span<const char* const> deviceLayers,
								 std::span<const char* const> deviceExtensions);

	[[nodiscard]] constexpr bool IsDebugMessengerInitialized() const noexcept
	{
		return m_DebugMessenger.has_value();
	}

	[[nodiscard]] constexpr bool IsLogicalDeviceInitialized() const noexcept
	{
		return m_LogicalDevice.has_value();
	}

	[[nodiscard]] constexpr vk::Instance GetVulkanInstance() const noexcept
	{
		return m_VulkanInstance;
	}

	[[nodiscard]] constexpr vk::DebugUtilsMessengerEXT GetDebugMessenger() const
	{
		return m_DebugMessenger.value();
	}

	[[nodiscard]] constexpr vk::Device GetLogicalDevice() const
	{
		return m_LogicalDevice.value();
	}

private:
	vk::DynamicLoader m_DynamicLoader;
	vk::Instance m_VulkanInstance;

	std::optional<vk::DebugUtilsMessengerEXT> m_DebugMessenger;
	std::optional<vk::Device> m_LogicalDevice;
};
