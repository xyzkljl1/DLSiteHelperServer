#include <QtCore/QCoreApplication>
#include "DBProxyServer.h"
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
	DBProxyServer server;
    return a.exec();
}
