#include <VulkanApp/MainWindow.h>

#include <QApplication>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcVk, "qt.vulkan")

int main(int argc, char **argv)
{
    QGuiApplication app{ argc, argv };

    QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

    QVulkanInstance vulkanInstance{};
    // enable the standard validation layers, when available
    //vulkanInstance.setLayers({ "VK_LAYER_KHRONOS_validation" });

    if (!vulkanInstance.create())
        qFatal("Failed to create Vulkan instance: %d", vulkanInstance.errorCode());


    MainWindow window;
    window.setVulkanInstance(&vulkanInstance);
    window.resize(1024, 768);
    window.show();

    return QGuiApplication::exec();
}
