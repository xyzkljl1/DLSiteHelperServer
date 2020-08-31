#include "MyFakeWindow.h"
#include <Windows.h>
MyFakeWindow::MyFakeWindow(QObject *parent)
	: QObject(parent)
{
	QSystemTrayIcon* icon = new QSystemTrayIcon(this);
	icon->setIcon(QIcon(":/icon.png"));
	icon->setToolTip("DLSiteHelper");
	icon->show();
	connect(icon, &QSystemTrayIcon::activated, this, &MyFakeWindow::onIconClicked);
}

MyFakeWindow::~MyFakeWindow()
{
}

void MyFakeWindow::onIconClicked(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::ActivationReason::DoubleClick)
	{
		HWND hw = GetConsoleWindow();
		ShowWindow(hw, visible);
		visible = !visible;
	}
}
