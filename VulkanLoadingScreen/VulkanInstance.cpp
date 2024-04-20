#include <VulkanLoadingScreen/VulkanInstance.h>

#include <optional>
#include <unordered_set>
#include <vector>

#include <fmt/core.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace
{
constexpr std::uint32_t VulkanVersion = VK_HEADER_VERSION_COMPLETE;

std::unordered_set<std::string> GetSupportedExtensions()
{
	const std::vector<vk::ExtensionProperties> extensionProperties =
		vk::enumerateInstanceExtensionProperties();

	std::unordered_set<std::string> extensions;
	for (const vk::ExtensionProperties& extension : extensionProperties)
	{
		extensions.emplace(extension.extensionName);
	}

	return extensions;
}

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

	const auto vkGetInstanceProcAddr =
	    m_DynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>(
	        "vkGetInstanceProcAddr");
	if (vkGetInstanceProcAddr == nullptr)
	{
		throw std::runtime_error{
			"Failed to find vkGetInstanceProcAddr symbol in vulkan library"
		};
	}
	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

	constexpr std::uint32_t ApplicationVersion = 1;
	constexpr vk::ApplicationInfo AppInfo{ "VulkanLoadingScreen",
										   ApplicationVersion, "", 0,
										   VulkanVersion };

	const std::unordered_set<std::string> supportedExtensions =
		GetSupportedExtensions();
	for (const char* const extension : vulkanExtensions)
	{
		if (!supportedExtensions.contains(extension))
		{
			fmt::println(stderr, "Extension {} is not supported", extension);
			throw std::runtime_error{ "Unsupported extension" };
		}
	}

	const vk::InstanceCreateInfo createInfo{
		vk::InstanceCreateFlags{},
		&AppInfo,
		static_cast<std::uint32_t>(vulkanLayers.size()),
		vulkanLayers.data(),
		static_cast<std::uint32_t>(vulkanExtensions.size()),
		vulkanExtensions.data()
	};

	m_VulkanInstance = vk::createInstance(createInfo, nullptr);

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
	constexpr vk::DebugUtilsMessengerCreateFlagsEXT MessengerCreateFlags{};

	constexpr vk::DebugUtilsMessageSeverityFlagsEXT MessageSeverityFlags =
	    vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
	    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
	    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

	constexpr vk::DebugUtilsMessageTypeFlagsEXT MessageTypeFlags =
	    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
	    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
	    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

	const vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{
		MessengerCreateFlags, MessageSeverityFlags, MessageTypeFlags,
		&DebugCallback, nullptr
	};
	m_DebugMessenger =
		m_VulkanInstance.createDebugUtilsMessengerEXT(debugMessengerCreateInfo);
}

void VulkanInstance::InitializeLogicalDevice(
    const std::span<const char* const> vulkanLayers,
    const std::span<const char* const> vulkanExtensions)
{
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

	// Optional value is checked in 'IsComplete' member function
	const vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		vk::DeviceQueueCreateFlags{}, queueIndices.GraphicsFamily.value(), 1,
		&QueuePriority
	};

	constexpr vk::PhysicalDeviceFeatures DeviceFeatures{};
	const vk::DeviceCreateInfo deviceInfo{
		vk::DeviceCreateFlags{},
		1,
		&deviceQueueCreateInfo,
		static_cast<std::uint32_t>(vulkanLayers.size()),
		vulkanLayers.data(),
		static_cast<std::uint32_t>(vulkanExtensions.size()),
		vulkanExtensions.data(),
		&DeviceFeatures
	};

	m_LogicalDevice = defaultDevice.createDevice(deviceInfo);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_LogicalDevice);
}
