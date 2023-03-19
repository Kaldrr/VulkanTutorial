#include <VulkanLoadingScreen/VulkanInstance.h>

#include <optional>

#include <fmt/core.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace
{
constexpr unsigned int VulkanVersion = VK_API_VERSION_1_3;

struct QueueFamilyIndices
{
public:
	std::optional<std::uint32_t> graphicsFamily{ std::nullopt };

	constexpr bool isComplete() const noexcept
	{
		return graphicsFamily.has_value();
	}
};

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    const VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* const pUserData)
{
	if (messageSeverity >=
	    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		fmt::print("VulkanDebug: {}\n", pCallbackData->pMessage);
	}

	// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/PFN_vkDebugUtilsMessengerCallbackEXT.html
	// The application *should* always return VK_FALSE.
	return VK_FALSE;
}

QueueFamilyIndices findQueueFamilies(
    const vk::PhysicalDevice& physicalDevice)
{
	QueueFamilyIndices queueFamilyIndices{};

	const std::vector<vk::QueueFamilyProperties> queueFamilies =
	    physicalDevice.getQueueFamilyProperties();

	for (std::uint32_t i{ 0 };
	     const vk::QueueFamilyProperties& queueProperties :
	     queueFamilies)
	{
		if (queueProperties.queueFlags &
		    vk::QueueFlags{ VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT })
		{
			queueFamilyIndices.graphicsFamily = i;
		}

		if (queueFamilyIndices.isComplete())
			break;
		++i;
	}

	return queueFamilyIndices;
}

} // namespace

;

VulkanInstance::VulkanInstance()
{
	// Needs to be valid for the lifetime of default dispatcher
	// destructor unloads the library from memory
	if (!m_DynamicLoader.success())
	{
		throw std::runtime_error{
			"Failed to dynamically load vulkan library"
		};
	}

	const PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr =
	    m_DynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>(
	        "vkGetInstanceProcAddr");
	if (vkGetInstanceProcAddr == nullptr)
	{
		throw std::runtime_error{
			"Failed to find vkGetInstanceProcAddr symbol in vulkan library"
		};
	}

	VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
}

VulkanInstance::~VulkanInstance() noexcept
{
	if (m_LogicalDevice.has_value())
	{
		m_LogicalDevice->destroy();
		m_LogicalDevice.reset();
	}

	if (m_VulkanInstance.has_value())
	{
		if (m_DebugMessenger.has_value())
		{
			m_VulkanInstance->destroy(*m_DebugMessenger);
			m_DebugMessenger.reset();
		}
		m_VulkanInstance->destroy();
		m_VulkanInstance.reset();
	}
}

void VulkanInstance::InitializeVulkanInstance(
    const std::span<const char* const> vulkanLayers,
    const std::span<const char* const> vulkanExtensions)
{
	constexpr vk::ApplicationInfo appInfo{ "VulkanLoadingScreen",
		                                   VulkanVersion, "No Engine",
		                                   VulkanVersion,
		                                   VulkanVersion };

	const vk::InstanceCreateInfo createInfo{
		vk::InstanceCreateFlags{},
		&appInfo,
		static_cast<std::uint32_t>(vulkanLayers.size()),
		vulkanLayers.data(),
		static_cast<std::uint32_t>(vulkanExtensions.size()),
		vulkanExtensions.data()
	};

	m_VulkanInstance = vk::createInstance(createInfo, nullptr);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_VulkanInstance);
}

void VulkanInstance::InitializeDebugMessenger()
{
	if (!IsVulkanInstanceInitialized())
	{
		fmt::print(stderr,
		           "{} called with uninitialized VulkanInstance",
					__FUNCTION__);
		return;
	}

	constexpr vk::DebugUtilsMessengerCreateFlagsEXT
	    messengerCreateFlags{};

	constexpr vk::DebugUtilsMessageSeverityFlagsEXT
	    messageSeverityFlags =
	        vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
	        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
	        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

	constexpr vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags =
	    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
	    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
	    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

	const vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{
		messengerCreateFlags, messageSeverityFlags, messageTypeFlags,
		&debugCallback, nullptr
	};
	m_DebugMessenger = m_VulkanInstance->createDebugUtilsMessengerEXT(
	    debugMessengerCreateInfo);
}

void VulkanInstance::InitializeLogicalDevice(
    const std::span<const char* const> vulkanLayers,
    const std::span<const char* const> vulkanExtensions)
{
	if (!IsVulkanInstanceInitialized())
	{
		fmt::print(stderr,
		           "{} called with uninitialized VulkanInstance",
		           __FUNCTION__);
		return;
	}

	const std::vector<vk::PhysicalDevice> physicalDevices =
	    m_VulkanInstance->enumeratePhysicalDevices();
	if (physicalDevices.empty())
	{
		throw std::runtime_error{
			"Failed to find any vulkan capable device!"
		};
	}

	// TODO: make some smarter device selection?
	constexpr std::size_t deviceIndex = 0;

	const vk::PhysicalDevice& defaultDevice =
	    physicalDevices.at(deviceIndex);

	const vk::PhysicalDeviceProperties deviceProperties{
		defaultDevice.getProperties()
	};
	const std::string_view deviceName{ deviceProperties.deviceName };
	fmt::print("Using device {}\n", deviceName);

	const QueueFamilyIndices queueIndices =
	    findQueueFamilies(defaultDevice);
	if (!queueIndices.isComplete())
	{
		throw std::runtime_error{
			"Failed to find expected queues on the device!"
		};
	}
	constexpr float QueuePriority{ 1.f };
	const vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		vk::DeviceQueueCreateFlags{},
		queueIndices.graphicsFamily.value(), 1, &QueuePriority
	};

	constexpr vk::PhysicalDeviceFeatures deviceFeatures{};
	const vk::DeviceCreateInfo deviceInfo{
		vk::DeviceCreateFlags{},
		1,
		&deviceQueueCreateInfo,
		static_cast<std::uint32_t>(vulkanLayers.size()),
		vulkanLayers.data(),
		static_cast<std::uint32_t>(vulkanExtensions.size()),
		vulkanExtensions.data(),
		&deviceFeatures
	};

	m_LogicalDevice = defaultDevice.createDevice(deviceInfo);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_LogicalDevice);
}