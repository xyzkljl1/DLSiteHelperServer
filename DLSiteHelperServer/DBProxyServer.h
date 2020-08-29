#pragma once
#include <QTcpServer>
#include <QStringList>
class DBProxyServer:public QTcpServer
{
	Q_OBJECT
public:
	DBProxyServer();
	~DBProxyServer();
protected:
	void onConnected();
	void onReceived(QTcpSocket* conn);
	void onReleased(QTcpSocket* conn);
	void sendStandardResponse(QTcpSocket* conn, const std::string& message);
	void sendFailResponse(QTcpSocket* conn, const std::string& message);

	std::string GetAllInvalidWork();
	std::string GetAllOverlapWork();
	void SyncLocalFile();

protected:
	QStringList local_dirs = {QString::fromLocal8Bit("D:/ASMR_archive/"),QString::fromLocal8Bit("D:/video/pic/") };
};

