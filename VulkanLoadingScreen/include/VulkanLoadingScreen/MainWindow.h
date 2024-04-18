#pragma once

#include <QObject>
#include <QVulkanWindow>

class [[nodiscard]] MainWindow : public QVulkanWindow
{
	Q_OBJECT

public:
	MainWindow();
	explicit MainWindow(QWindow* parent);

	[[nodiscard]] QVulkanWindowRenderer* createRenderer() override;
};
