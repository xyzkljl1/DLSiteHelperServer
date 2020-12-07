#pragma once
#include "DLSiteClient.h"
#include <QStringList>
#include <QTimer>
#include "tufao/HttpServer"
#include "Tufao/HttpServerRequest"
#include "Tufao/HttpServerResponse"
#include "Tufao/Headers"

class DBProxyServer:public Tufao::HttpServer
{
	Q_OBJECT
public:
	DBProxyServer(QObject* parent);
	~DBProxyServer();
	static QRegExp GetWorkNameExp();
protected:
	void HandleRequest(Tufao::HttpServerRequest &request, Tufao::HttpServerResponse &response);
	void ReplyText(Tufao::HttpServerResponse & response, const Tufao::HttpResponseStatus&status, const QString & message);

	
	QString GetAllInvalidWork();
	QString GetAllOverlapWork();
	QString UpdateBoughtItems(const QByteArray& data);

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
	QTimer daily_timer;
};

