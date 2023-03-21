#include <VulkanLoadingScreen/MainWindow.h>
#include <VulkanLoadingScreen/VulkanRenderer.h>

MainWindow::MainWindow()
    : MainWindow{nullptr}
{}

MainWindow::MainWindow(QWindow *const parent)
    : QVulkanWindow{ parent }
{
}

MainWindow::~MainWindow()
{
}

QVulkanWindowRenderer* MainWindow::createRenderer()
{
    // TODO: fix multisampling
    return new VulkanRenderer{this, false};
}

