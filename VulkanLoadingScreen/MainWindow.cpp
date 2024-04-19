#include <VulkanLoadingScreen/MainWindow.h>
#include <VulkanLoadingScreen/VulkanRenderer.h>

namespace
{
void SetDeviceFeatures(VkPhysicalDeviceFeatures& deviceFeature)
{
	deviceFeature.samplerAnisotropy = VK_TRUE;
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
    // TODO: fix multisampling
    return new VulkanRenderer{this, true};
}
