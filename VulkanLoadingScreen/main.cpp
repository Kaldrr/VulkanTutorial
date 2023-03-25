#include <VulkanLoadingScreen/MainWindow.h>

#include <QApplication>
#include <QLoggingCategory>

#include <VulkanLoadingScreen/VulkanInstance.h>

#include <concepts>

// Q_LOGGING_CATEGORY(lcVk, "qt.vulkan")

namespace
{
const QByteArrayList VulkanLayers{
#ifndef NDEBUG
	"VK_LAYER_KHRONOS_validation"
#endif
};

// Taken from default initialized + created QVulkanInstance
const QByteArrayList VulkanExtensions{
	"VK_KHR_surface",
	"VK_KHR_portability_enumeration",
	"VK_EXT_debug_report",
	"VK_KHR_win32_surface",
#ifndef NDEBUG
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};

// QByteArrayList can't be cast to a const char*...
// sizeof(QByteArray) == 24, we need conversions when passing to vulkan
std::vector<const char*> ToVector(const QByteArrayList& list)
{
	using std::begin, std::end, std::size;

	std::vector<const char*> outVector{};
	outVector.reserve(size(list));
	std::transform(begin(list), end(list),
	               std::back_inserter(outVector),
	               std::mem_fn(&QByteArray::constData));

	return outVector;
}
} // namespace

int main(int argc, char** argv)
{
	// QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));
	QGuiApplication app{ argc, argv };
	QVulkanInstance qtVulkanInstance{};

	constexpr bool UseExternalVulkan = true;
	[[maybe_unused]] std::optional<VulkanInstance> vulkanInstance{};
	if constexpr (UseExternalVulkan)
	{
		VulkanInstance& vulkan = vulkanInstance.emplace();

		const std::vector<const char*> vulkanLayersVector =
		    ToVector(VulkanLayers);
		const std::vector<const char*> vulkanExtensionsVector =
		    ToVector(VulkanExtensions);

		vulkan.InitializeVulkanInstance(vulkanLayersVector,
		                                vulkanExtensionsVector);
		vulkan.InitializeDebugMessenger();
		// QVulkanWindow ALWAYS creates it's own device
		// vulkanInstance->InitializeLogicalDevice(vulkanLayersVector,
		//                                       vulkanExtensionsVector);
		qtVulkanInstance.setVkInstance(vulkan.GetVulkanInstance());
	}
	qtVulkanInstance.setApiVersion(QVersionNumber{ 1, 3, 0 });
	qtVulkanInstance.setExtensions(VulkanExtensions);
	qtVulkanInstance.setLayers(VulkanLayers);

	if (!qtVulkanInstance.create())
		qFatal("Failed to create Vulkan instance: %d",
		       qtVulkanInstance.errorCode());

	const int returnCode = [&qtVulkanInstance] {
		try
		{
			MainWindow window;
			window.setVulkanInstance(&qtVulkanInstance);
			window.resize(800, 800);
			window.show();

			return QGuiApplication::exec();
		}
		catch (const std::exception& e)
		{
			qFatal("Fatal error: %s", e.what());
		}
		catch (...)
		{
			qFatal("Unknown fatal error");
		}
		return -1;
	}();
	// Destroy both MainWindow and QVulkanInstance
	// So they release all their Vulkan resource first
	qtVulkanInstance.destroy();

	// QVulkanInstance doesn't destroy VulkanInstance it does not own
	vulkanInstance.reset();

	return returnCode;
}
