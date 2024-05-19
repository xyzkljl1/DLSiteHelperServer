#include "MyFakeWindow.h"
#include <Windows.h>
#include <QMenu>
#include <QAction>
MyFakeWindow::MyFakeWindow(QObject *parent)
	: QObject(parent)
{
	QSystemTrayIcon* icon = new QSystemTrayIcon(this);
	icon->setIcon(QIcon(":/icon.png"));
	icon->setToolTip("DLSiteHelper");
	icon->show();
	connect(icon, &QSystemTrayIcon::activated, this, &MyFakeWindow::onIconClicked);
	QMenu *menu=new QMenu();
	QAction *exit_action=new QAction(QString::fromLocal8Bit("退出"),menu);
	menu->addAction(exit_action);
	icon->setContextMenu(menu);
	connect(exit_action, &QAction::triggered, this, &MyFakeWindow::signalClose);
}

MyFakeWindow::~MyFakeWindow()
{
}

void MyFakeWindow::onIconClicked(QSystemTrayIcon::ActivationReason reason)
{
	if (reason == QSystemTrayIcon::ActivationReason::DoubleClick)
	{
		HWND hw = GetConsoleWindow();
		ShowWindow(hw, visible? SW_SHOW: SW_HIDE);
		visible = !visible;
	}
}
