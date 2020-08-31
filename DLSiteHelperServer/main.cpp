#include <QApplication>
#include "MyFakeWindow.h"
#include "DBProxyServer.h"
int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	DBProxyServer server;
	MyFakeWindow window;
    return a.exec();
}
