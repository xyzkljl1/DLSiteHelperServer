#include <QApplication>
#include <ranges>
#include "MyFakeWindow.h"
#include "DLSiteHelperServer.h"
import Util;
using namespace Util;
int main(int argc, char* argv[])
{
	QApplication a(argc, argv);
	LoadConfigFromFile(QApplication::applicationDirPath() + "/config.json");
	DLSiteHelperServer server(&a);
	//用char*构造set<std::string>似乎没有问题？
	auto args = std::set<std::string>(argv + 1, argv + argc);
	if (args.count("-u"))
	{
		setvbuf(stdout, NULL, _IONBF, 0);
		setvbuf(stderr, NULL, _IONBF, 0);
	}
	if (args.count("-w"))
	{
		MyFakeWindow window;
		QObject::connect(&window, &MyFakeWindow::signalClose, &a, &QApplication::quit);
		return a.exec();
	}
	return a.exec();
}
