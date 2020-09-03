#include <QApplication>
#include "MyFakeWindow.h"
#include "DBProxyServer.h"
int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	DBProxyServer server;
	MyFakeWindow window;
	QObject::connect(&window, &MyFakeWindow::signalClose, &a, &QApplication::quit);
    return a.exec();
}
