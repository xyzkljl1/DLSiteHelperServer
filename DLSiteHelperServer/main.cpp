#include <QApplication>
#include "MyFakeWindow.h"
#include "DBProxyServer.h"
int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	DBProxyServer server;
	std::set<std::string> args;
	for (int i = 1; i < argc; ++i)
		args.insert(argv[i]);
	if (args.count("-u"))
		setvbuf(stdout, NULL, _IONBF, 0);
	if (args.count("-w"))
	{
		MyFakeWindow window;
		QObject::connect(&window, &MyFakeWindow::signalClose, &a, &QApplication::quit);
		return a.exec();
	}
	return a.exec();
}
