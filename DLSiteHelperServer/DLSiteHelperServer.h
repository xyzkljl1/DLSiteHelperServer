#pragma once
#include "DLSiteClient.h"
#include "DLConfig.h"
#include <QStringList>
#include <QTimer>
#include "tufao/HttpServer"
#include "Tufao/HttpServerRequest"
#include "Tufao/HttpServerResponse"
#include "Tufao/Headers"

class DLSiteHelperServer:public Tufao::HttpServer
{
	Q_OBJECT
public:
	DLSiteHelperServer(QObject* parent);
	~DLSiteHelperServer();
	static QRegExp GetWorkNameExp();
protected:
	void HandleRequest(Tufao::HttpServerRequest &request, Tufao::HttpServerResponse &response);
	void ReplyText(Tufao::HttpServerResponse & response, const Tufao::HttpResponseStatus&status, const QString & message);
	
	QString GetAllInvalidWork();
	QString GetAllOverlapWork();
	QString UpdateBoughtItems(const QByteArray& data);

	void SyncLocalFileToDB();
	void DownloadAll(const QByteArray& cookie, const QByteArray& user_agent);
	void RenameLocal();
	QStringList GetLocalFiles(const QStringList& root);
protected:
	DLSiteClient client;
	QTimer daily_timer;
};

