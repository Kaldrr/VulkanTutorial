#pragma once

#include <vulkan/vulkan.hpp>

#include <optional>
#include <span>

class VulkanInstance
{
public:
	VulkanInstance();
	~VulkanInstance() noexcept;

	void InitializeVulkanInstance(
	    std::span<const char* const> vulkanLayers,
	    std::span<const char* const> vulkanExtensions);

	void InitializeDebugMessenger();

	void InitializeLogicalDevice(
	    std::span<const char* const> vulkanLayers,
	    std::span<const char* const> vulkanExtensions);

	[[nodiscard]] constexpr bool IsVulkanInstanceInitialized()
		const noexcept
	{
		return m_VulkanInstance.has_value();
	}

	[[nodiscard]] constexpr bool IsDebugMessengerInitialized()
		const noexcept
	{
		return m_DebugMessenger.has_value();
	}

	[[nodiscard]] constexpr bool IsLogicalDeviceInitialized()
		const noexcept
	{
		return m_LogicalDevice.has_value();
	}

	[[nodiscard]] constexpr vk::Instance GetVulkanInstance() const
	{
		return m_VulkanInstance.value();
	}

	[[nodiscard]] constexpr vk::DebugUtilsMessengerEXT
	GetDebugMessenger() const
	{
		return m_DebugMessenger.value();
	}

	[[nodiscard]] constexpr vk::Device GetLogicalDevice() const
	{
		return m_LogicalDevice.value();
	}

private:
	vk::DynamicLoader m_DynamicLoader{};

	std::optional<vk::Instance> m_VulkanInstance{};
	std::optional<vk::DebugUtilsMessengerEXT> m_DebugMessenger{};
	std::optional<vk::Device> m_LogicalDevice{};
};
