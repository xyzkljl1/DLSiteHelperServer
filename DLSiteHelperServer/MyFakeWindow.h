#pragma once
#include <QObject>
#include <QSystemTrayIcon>
class MyFakeWindow : public QObject
{
	Q_OBJECT

public:
	MyFakeWindow(QObject *parent=nullptr);
	~MyFakeWindow();
signals:
	void signalClose();
protected:
	void onIconClicked(QSystemTrayIcon::ActivationReason);	
	bool visible=true;
};
