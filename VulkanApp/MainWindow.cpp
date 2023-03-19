#include <VulkanApp/MainWindow.h>
#include <VulkanApp/VulkanRenderer.h>

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
    return new VulkanRenderer{this, true};
}

