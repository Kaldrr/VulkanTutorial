#pragma once

#include <QObject>
#include <QVulkanWindow>

class [[nodiscard]] MainWindow : public QVulkanWindow
{
	// NOLINTBEGIN
	Q_OBJECT
	// NOLINTEND
public:
	MainWindow();
	explicit MainWindow(QWindow* parent);

	[[nodiscard]] QVulkanWindowRenderer* createRenderer() override;
};
