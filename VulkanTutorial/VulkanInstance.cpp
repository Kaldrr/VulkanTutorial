#include <VulkanTutorial/VulkanInstance.h>

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
	std::optional<std::uint32_t> PresentationFamily{ std::nullopt };

	constexpr bool IsComplete() const noexcept
	{
		return GraphicsFamily.has_value() && PresentationFamily.has_value();
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

QueueFamilyIndices FindQueueFamilies(const vk::PhysicalDevice physicalDevice,
									 const vk::SurfaceKHR vulkanSurface)
{
	QueueFamilyIndices queueFamilyIndices{};

	const std::vector<vk::QueueFamilyProperties> queueFamilies =
	    physicalDevice.getQueueFamilyProperties();

	for (std::uint32_t idx{ 0 };
	     const vk::QueueFamilyProperties& queueProperties : queueFamilies)
	{
		if (queueProperties.queueFlags & vk::QueueFlagBits::eGraphics)
		{
			queueFamilyIndices.GraphicsFamily = idx;
		}

		if (physicalDevice.getSurfaceSupportKHR(idx, vulkanSurface) == vk::True)
		{
			queueFamilyIndices.PresentationFamily = idx;
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
		.pApplicationName   = "VulkanTutorialQt",
		.applicationVersion = ApplicationVersion,
		.apiVersion         = VulkanVersion,
	};

	const std::set<std::string>& supportedLayerExtensions =
		vk::getInstanceExtensions();
	for (const char* const layerExtension : vulkanExtensions)
	{
		if (!supportedLayerExtensions.contains(layerExtension))
		{
			fmt::println(stderr, "Layer extension {} is not supported",
						 layerExtension);
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
	//m_Device.destroy(m_CommandPool);
	m_Device.destroy();
	m_VulkanInstance.destroy(m_DebugMessenger);
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
			.pfnUserCallback = &DebugCallback,
		});
}

void VulkanInstance::InitializeDevice(
    const std::span<const char* const> deviceExtensions,
    const vk::SurfaceKHR vulkanSurface)
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
	m_PhyiscalDevice                  = physicalDevices.at(DeviceIndex);

	const vk::PhysicalDeviceProperties deviceProperties{
		m_PhyiscalDevice.getProperties()
	};
	const std::string_view deviceName{ deviceProperties.deviceName };
	fmt::print("Using device {}\n", deviceName);

	const QueueFamilyIndices queueIndices =
		FindQueueFamilies(m_PhyiscalDevice, vulkanSurface);
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

	// Device layers are deprecated, only accept extensions
	constexpr vk::PhysicalDeviceFeatures DeviceFeatures{
		.samplerAnisotropy = vk::True,
	};
	m_Device    = m_PhyiscalDevice.createDevice(vk::DeviceCreateInfo{
		   .queueCreateInfoCount = 1,
		   .pQueueCreateInfos    = &deviceQueueCreateInfo,
		   .enabledExtensionCount =
			static_cast<std::uint32_t>(deviceExtensions.size()),
		   .ppEnabledExtensionNames = deviceExtensions.data(),
		   .pEnabledFeatures        = &DeviceFeatures });
	m_WorkQueue = m_Device.getQueue(queueIndices.GraphicsFamily.value(), 0);

	m_CommandPool = m_Device.createCommandPool(vk::CommandPoolCreateInfo{
		.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		.queueFamilyIndex = queueIndices.GraphicsFamily.value() });

	// VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device);
}
