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
	static QRegExp GetWorkNameExpSep();
protected:
	void HandleRequest(Tufao::HttpServerRequest &request, Tufao::HttpServerResponse &response);
	void ReplyText(Tufao::HttpServerResponse & response, const Tufao::HttpResponseStatus&status, const QString & message);
	
	QString GetAllInvalidWorksString();
	QString GetAllOverlapWorksString();
	QString UpdateBoughtItems(const QByteArray& data);

	std::map<std::string, std::set<std::string>> GetAllOverlapWorks();
	void SyncLocalFileToDB();
	void DownloadAll(const QByteArray& cookie, const QByteArray& user_agent);
	void RenameLocal();
	QStringList GetLocalFiles(const QStringList& root);
	void EliminateOTMWorks();
protected:
	DLSiteClient client;
	QTimer daily_timer;
};

