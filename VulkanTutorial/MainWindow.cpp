#include <VulkanTutorial/MainWindow.h>
#include <VulkanTutorial/VulkanRenderer.h>

namespace
{
void SetDeviceFeatures(VkPhysicalDeviceFeatures& deviceFeature)
{
	deviceFeature.samplerAnisotropy = vk::True;
}
} // namespace

MainWindow::MainWindow()
	: MainWindow{ nullptr }
{
}

MainWindow::MainWindow(QWindow* const parent)
    : QVulkanWindow{ parent }
{
    setEnabledFeaturesModifier(&SetDeviceFeatures);
}

QVulkanWindowRenderer* MainWindow::createRenderer()
{
	constexpr bool MSAAEnabled = true;
	// This needs to be a raw pointer return
	// It's ok as we give it a parent, so the MainWindow will take care of managing
	// it

	// NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
	return new VulkanRenderer{ *this, MSAAEnabled };
}
