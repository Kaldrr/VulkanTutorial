#include <VulkanLoadingScreen/VulkanInstance.h>

#include <optional>
#include <unordered_set>
#include <vector>

#include <fmt/core.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace
{
constexpr std::uint32_t VulkanVersion = VK_API_VERSION_1_3;

std::unordered_set<std::string> GetSupportedExtensions()
{
	VkResult result{};

	/*
	 * From the link above:
	 * If `pProperties` is NULL, then the number of extensions properties
	 * available is returned in `pPropertyCount`.
	 * 	 * Basically, gets the number of extensions.
	 */
	std::uint32_t count = 0;

	result = vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
	if (result != VK_SUCCESS)
	{
		// Throw an exception or log the error
	}

	std::vector<VkExtensionProperties> extensionProperties(count);

	// Get the extensions
	// Doesn't seem to have VulkanCpp equivalent?
	result = vkEnumerateInstanceExtensionProperties(nullptr, &count,
													extensionProperties.data());
	if (result != VK_SUCCESS)
	{
		// Throw an exception or log the error
	}

	std::unordered_set<std::string> extensions;
	for (auto& extension : extensionProperties)
	{
		extensions.insert(extension.extensionName);
	}

	return extensions;
}

struct QueueFamilyIndices
{
public:
	std::optional<std::uint32_t> graphicsFamily{ std::nullopt };

	constexpr bool IsComplete() const noexcept
	{
		return graphicsFamily.has_value();
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

	for (std::uint32_t i{ 0 };
		 const vk::QueueFamilyProperties& queueProperties : queueFamilies)
	{
		if (queueProperties.queueFlags &
		    vk::QueueFlags{ VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT })
		{
			queueFamilyIndices.graphicsFamily = i;
		}

		if (queueFamilyIndices.IsComplete())
			break;
		++i;
	}

	return queueFamilyIndices;
}

} // namespace

VulkanInstance::VulkanInstance()
{
	// Needs to be valid for the lifetime of default dispatcher
	// destructor unloads the library from memory
	if (!m_DynamicLoader.success())
	{
		throw std::runtime_error{ "Failed to dynamically load vulkan library" };
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
	}

	if (m_VulkanInstance.has_value())
	{
		if (m_DebugMessenger.has_value())
		{
			m_VulkanInstance->destroy(*m_DebugMessenger);
		}
		m_VulkanInstance->destroy();
	}
}

void VulkanInstance::InitializeVulkanInstance(
    const std::span<const char* const> vulkanLayers,
    const std::span<const char* const> vulkanExtensions)
{
	constexpr std::uint32_t ApplicationVersion = 1;
	constexpr vk::ApplicationInfo appInfo{ "VulkanLoadingScreen",
										   ApplicationVersion, "", 0,
										   VulkanVersion };

	const std::unordered_set<std::string> extensions = GetSupportedExtensions();
	for (const char* const extension : vulkanExtensions)
	{
		if (!extensions.contains(extension))
		{
			fmt::println(stderr, "Extension {} is not supported", extension);
			throw std::runtime_error{"Unsupported extension"};
		}
	}

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
		fmt::print(stderr, "{} called with uninitialized VulkanInstance",
		           __FUNCTION__);
		return;
	}

	constexpr vk::DebugUtilsMessengerCreateFlagsEXT messengerCreateFlags{};

	constexpr vk::DebugUtilsMessageSeverityFlagsEXT messageSeverityFlags =
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

	constexpr vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags =
	    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
	    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
	    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

	const vk::DebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{
		messengerCreateFlags, messageSeverityFlags, messageTypeFlags,
		&DebugCallback, nullptr
	};
	m_DebugMessenger =
		m_VulkanInstance->createDebugUtilsMessengerEXT(debugMessengerCreateInfo);
}

void VulkanInstance::InitializeLogicalDevice(
    const std::span<const char* const> vulkanLayers,
    const std::span<const char* const> vulkanExtensions)
{
	if (!IsVulkanInstanceInitialized())
	{
		fmt::print(stderr, "{} called with uninitialized VulkanInstance",
		           __FUNCTION__);
		return;
	}

	const std::vector<vk::PhysicalDevice> physicalDevices =
	    m_VulkanInstance->enumeratePhysicalDevices();
	if (physicalDevices.empty())
	{
		throw std::runtime_error{ "Failed to find any vulkan capable device!" };
	}

	// TODO: make some smarter device selection?
	constexpr std::size_t deviceIndex = 0;

	const vk::PhysicalDevice& defaultDevice = physicalDevices.at(deviceIndex);

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
	constexpr float QueuePriority{ 1.f };
	const vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
		vk::DeviceQueueCreateFlags{}, queueIndices.graphicsFamily.value(), 1,
		&QueuePriority
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
