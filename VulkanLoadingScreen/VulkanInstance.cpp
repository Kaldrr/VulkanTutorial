#include <VulkanLoadingScreen/VulkanInstance.h>

#include <optional>
#include <set>
#include <vector>

#include <fmt/core.h>

#include <vulkan/vulkan_extension_inspection.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace
{
constexpr std::uint32_t VulkanVersion = VK_HEADER_VERSION_COMPLETE;

struct QueueFamilyIndices
{
public:
	std::optional<std::uint32_t> GraphicsFamily{ std::nullopt };

	constexpr bool IsComplete() const noexcept
	{
		return GraphicsFamily.has_value();
	}
};

VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              [[maybe_unused]] const VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT* const pCallbackData,
              [[maybe_unused]] void* const pUserData)
{
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		fmt::print("VulkanDebug: {}\n", pCallbackData->pMessage);
	}

	// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/PFN_vkDebugUtilsMessengerCallbackEXT.html
	// The application *should* always return VK_FALSE.
	return VK_FALSE;
}

QueueFamilyIndices FindQueueFamilies(const vk::PhysicalDevice physicalDevice)
{
	QueueFamilyIndices queueFamilyIndices{};

	const std::vector<vk::QueueFamilyProperties> queueFamilies =
	    physicalDevice.getQueueFamilyProperties();

	for (std::uint32_t idx{ 0 };
	     const vk::QueueFamilyProperties& queueProperties : queueFamilies)
	{
		if (queueProperties.queueFlags &
		    vk::QueueFlags{ VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT })
		{
			queueFamilyIndices.GraphicsFamily = idx;
		}

		if (queueFamilyIndices.IsComplete())
		{
			break;
		}
		++idx;
	}

	return queueFamilyIndices;
}

} // namespace

VulkanInstance::VulkanInstance(const std::span<const char* const> vulkanLayers,
							   const std::span<const char* const> vulkanExtensions)
{
	// Needs to be valid for the lifetime of default dispatcher
	// destructor unloads the library from memory
	if (!m_DynamicLoader.success())
	{
		throw std::runtime_error{ "Failed to dynamically load vulkan library" };
	}
	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_DynamicLoader);

	constexpr std::uint32_t ApplicationVersion = 1;
	constexpr vk::ApplicationInfo AppInfo{
		.pApplicationName   = "VulkanLoadingScreen",
		.applicationVersion = ApplicationVersion,
		.apiVersion         = VulkanVersion,
	};

	const std::set<std::string>& supportedLayerExtensions = vk::getInstanceExtensions();
	for (const char* const layerExtension : vulkanExtensions)
	{
		if (!supportedLayerExtensions.contains(layerExtension))
		{
			fmt::println(stderr, "Layer extension {} is not supported", layerExtension);
			// throw std::runtime_error{ "Unsupported extension" };
		}
	}

	m_VulkanInstance = vk::createInstance(vk::InstanceCreateInfo{
		.pApplicationInfo    = &AppInfo,
		.enabledLayerCount   = static_cast<std::uint32_t>(vulkanLayers.size()),
		.ppEnabledLayerNames = vulkanLayers.data(),
		.enabledExtensionCount =
			static_cast<std::uint32_t>(vulkanExtensions.size()),
		.ppEnabledExtensionNames = vulkanExtensions.data(),
	});

	VULKAN_HPP_DEFAULT_DISPATCHER.init(m_VulkanInstance);
}

VulkanInstance::~VulkanInstance() noexcept
{
	if (m_LogicalDevice.has_value())
	{
		m_LogicalDevice->destroy();
	}

	if (m_DebugMessenger.has_value())
	{
		m_VulkanInstance.destroy(*m_DebugMessenger);
	}
	m_VulkanInstance.destroy();
}

void VulkanInstance::InitializeDebugMessenger()
{
	m_DebugMessenger = m_VulkanInstance.createDebugUtilsMessengerEXT(
		vk::DebugUtilsMessengerCreateInfoEXT{
			.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
							   vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
							   vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
			.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
						   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
						   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
			.pfnUserCallback = &DebugCallback });
}

void VulkanInstance::InitializeLogicalDevice(
    const std::span<const char* const> deviceLayers,
    const std::span<const char* const> deviceExtensions)
{
	const std::set<std::string>& supportedExtensions = vk::getDeviceExtensions();
	for (const char* const deviceExtension : deviceExtensions)
	{
		if (!supportedExtensions.contains(deviceExtension))
		{
			fmt::println(stderr, "Device extension {} is not supported",
						 deviceExtension);
			// throw std::runtime_error{ "Unsupported extension" };
		}
	}

	const std::vector<vk::PhysicalDevice> physicalDevices =
		m_VulkanInstance.enumeratePhysicalDevices();
	if (physicalDevices.empty())
	{
		throw std::runtime_error{ "Failed to find any vulkan capable device!" };
	}

	// TODO: make some smarter device selection?
	constexpr std::size_t DeviceIndex = 0;

	const vk::PhysicalDevice& defaultDevice = physicalDevices.at(DeviceIndex);

	const vk::PhysicalDeviceProperties deviceProperties{
		defaultDevice.getProperties()
	};
	const std::string_view deviceName{ deviceProperties.deviceName };
	fmt::print("Using device {}\n", deviceName);

	const QueueFamilyIndices queueIndices = FindQueueFamilies(defaultDevice);
	if (!queueIndices.IsComplete())
	{
		throw std::runtime_error{ "Failed to find expected queues on the device!" };
	}

	constexpr float QueuePriority{ 1.F };
	const vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		.queueFamilyIndex = queueIndices.GraphicsFamily.value(),
		.queueCount       = 1,
		.pQueuePriorities = &QueuePriority
	};

	constexpr vk::PhysicalDeviceFeatures DeviceFeatures{};
	m_LogicalDevice = defaultDevice.createDevice(vk::DeviceCreateInfo{
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos    = &deviceQueueCreateInfo,
		.enabledLayerCount    = static_cast<std::uint32_t>(deviceLayers.size()),
		.ppEnabledLayerNames  = deviceLayers.data(),
		.enabledExtensionCount =
			static_cast<std::uint32_t>(deviceExtensions.size()),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures        = &DeviceFeatures });

	VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_LogicalDevice);
}
