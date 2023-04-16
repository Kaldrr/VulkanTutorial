#include <VulkanLoadingScreen/MainWindow.h>
#include <VulkanLoadingScreen/VulkanRenderer.h>

namespace
{
void setDeviceFeatures(VkPhysicalDeviceFeatures& deviceFeature)
{
	deviceFeature.samplerAnisotropy = VK_TRUE;
}
}

MainWindow::MainWindow()
    : MainWindow{nullptr}
{}

MainWindow::MainWindow(QWindow *const parent)
    : QVulkanWindow{ parent }
{
	setEnabledFeaturesModifier(&setDeviceFeatures);
}

MainWindow::~MainWindow()
{
}

QVulkanWindowRenderer* MainWindow::createRenderer()
{
    // TODO: fix multisampling
    return new VulkanRenderer{this, true};
}

