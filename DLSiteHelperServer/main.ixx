#include <QApplication>
#include <ranges>
#include "MyFakeWindow.h"
#include "DLSiteHelperServer.h"
//使用import会让IntelliSense爆炸导致VS特别卡，需要暂时关闭IntelliSense
import Util;
int main(int argc, char* argv[])
{
	QApplication a(argc, argv);
	LoadConfigFromFile(QApplication::applicationDirPath() + "/config.json");
	DLSiteHelperServer server(&a);
	//auto args = std::set<std::string>(argv + 1, argv + argc);
	//ranges还是比C#差多了
	auto args = std::set<char*>(argv, argv + argc)
		| std::views::transform([](auto arg) { return std::string(arg); })
		| std::views::drop(1)
		| std::ranges::to<std::set<std::string>>();
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
