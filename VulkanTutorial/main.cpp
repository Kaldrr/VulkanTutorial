#include <VulkanTutorial/MainWindow.h>
#include <VulkanTutorial/VulkanInstance.h>

#include <QApplication>
#include <QByteArray>
#include <QByteArrayList>

namespace
{
// NOLINTBEGIN(cert-err58-cpp)
const QByteArrayList VulkanLayers{
#ifndef NDEBUG
	"VK_LAYER_KHRONOS_validation"
#endif
};

// Taken from default initialized + created QVulkanInstance
const QByteArrayList VulkanExtensions{
	"VK_KHR_surface",
	"VK_KHR_portability_enumeration",
#ifdef Q_OS_LINUX
	"VK_KHR_xcb_surface",
	"VK_KHR_wayland_surface",
#elif defined(Q_OS_WIN32)
	"VK_KHR_win32_surface",
#endif
#ifndef NDEBUG
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
};
// NOLINTEND(cert-err58-cpp)

// QByteArrayList can't be cast to a const char* :(
// sizeof(QByteArray) == 24, we need conversions when passing to vulkan
std::vector<const char*> ToVector(const QByteArrayList& list)
{
	using std::begin, std::end, std::size;

	std::vector<const char*> outVector{};
	outVector.reserve(static_cast<std::size_t>(size(list)));
	std::transform(begin(list), end(list), std::back_inserter(outVector),
	               std::mem_fn(&QByteArray::constData));

	return outVector;
}
} // namespace

int main(int argc, char** argv)
{
	const QGuiApplication app{ argc, argv };
	QVulkanInstance qtVulkanInstance{};

	// TODO: Using the built-in Qt Vulkan doesn't work
	// crashes when initializing vk::Device from VkDevice
	constexpr bool UseExternalVulkan = true;
	[[maybe_unused]] std::optional<VulkanInstance> vulkanInstance{};
	if (UseExternalVulkan)
	{
		VulkanInstance& vulkan = vulkanInstance.emplace(ToVector(VulkanLayers),
														ToVector(VulkanExtensions));
#ifndef NDEBUG
		vulkan.InitializeDebugMessenger();
#endif
		qtVulkanInstance.setVkInstance(vulkan.GetVulkanInstance());
	}

	qtVulkanInstance.setApiVersion(
		QVersionNumber{ VK_API_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE),
						VK_API_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE),
						VK_API_VERSION_PATCH(VK_HEADER_VERSION_COMPLETE) });
	qtVulkanInstance.setExtensions(VulkanExtensions);
	qtVulkanInstance.setLayers(VulkanLayers);
	if (!qtVulkanInstance.create())
	{
		qFatal()
			<< "Failed to create Vulkan instance:" << qtVulkanInstance.errorCode();
		return -1;
	}
	const int returnCode = [&qtVulkanInstance] {
		try
		{
			constexpr QSize StartingWindowSize{ 800, 800 };
			MainWindow window{};
			window.setVulkanInstance(&qtVulkanInstance);
			window.resize(StartingWindowSize);
			window.show();
			window.setVisibility(QWindow::Visibility::Windowed);

			return QGuiApplication::exec();
		}
		catch (const std::exception& e)
		{
			qFatal() << "Fatal error:" << e.what();
		}
		catch (...)
		{
			qFatal() << "Unknown fatal error";
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
