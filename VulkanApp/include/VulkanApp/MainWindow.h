#pragma once

#include <QVulkanWindow>

class MainWindow : public QVulkanWindow
{
    Q_OBJECT

public:
    MainWindow();
    explicit MainWindow(QWindow *parent);
    ~MainWindow();

    QVulkanWindowRenderer* createRenderer() override;
};
