#pragma once
#include "DLSiteClient.h"
#include <QTcpServer>
#include <QStringList>
class DBProxyServer:public QTcpServer
{
	Q_OBJECT
public:
	DBProxyServer();
	~DBProxyServer();
	static QRegExp GetWorkNameExp();
protected:
	void onConnected();
	void onReceived(QTcpSocket* conn);
	void onReleased(QTcpSocket* conn);

	void sendStandardResponse(QTcpSocket* conn, const std::string& message);
	void sendFailResponse(QTcpSocket* conn, const std::string& message);
	std::string GetAllInvalidWork();
	std::string GetAllOverlapWork();
	std::string UpdateBoughtItems(const QByteArray& data);

	void SyncLocalFile();

	void DownloadAll(const QByteArray& cookie);
	void RenameLocal();
protected:
	QStringList local_dirs = {
		QString::fromLocal8Bit("D:/ASMR_archive/"),
		QString::fromLocal8Bit("D:/ASMR_cant_archive/"),
		QString::fromLocal8Bit("D:/ASMR_c/"),
		QString::fromLocal8Bit("D:/video/pic/"),
		QString::fromLocal8Bit("D:/video/anime/"),
		QString::fromLocal8Bit("D:/video/exe/"),
		QString::fromLocal8Bit("D:/IDMDownload/avater/")
		 };
	//只参与rename,不视作已下载
	QStringList local_tmp_dirs = {
		QString::fromLocal8Bit("D:/IDMDownload/tmp/")
	};
	DLSiteClient client;
};

