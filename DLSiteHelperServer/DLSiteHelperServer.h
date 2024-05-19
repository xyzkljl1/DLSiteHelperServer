#pragma once
#include "DLSiteClient.h"
#include "DLConfig.h"
#include <QStringList>
#include <QTimer>
#include <QRegExp>
#include <QtHttpServer/QHttpServer>

class DLSiteHelperServer:public QObject
{
	Q_OBJECT
public:
	DLSiteHelperServer(QObject* parent);
	~DLSiteHelperServer();
	static QRegExp GetWorkNameExpSep();
	static QString GetIDFromDirName(QString dir);
protected:
	void HandleRequest(const QHttpServerRequest& request, QHttpServerResponder&& responseresponse);
	void ReplyText(QHttpServerResponder&& responder, const QHttpServerResponder::StatusCode& status = QHttpServerResponder::StatusCode::Ok, const QString& message = "Success");
	
	QString GetAllInvalidWorksString();
	QString GetAllOverlapWorksString();
	QString UpdateBoughtItems(const QByteArray& data);

	std::map<std::string, std::set<std::string>> GetAllOverlapWorks();
	void DailyTask();
	void SyncLocalFileToDB();
	void FetchWorkInfo(int limit=100);//从DLSite获取workinfo并存入数据库
	void DownloadAll(const QByteArray& cookie, const QByteArray& user_agent);
	void RenameLocal();
	QStringList GetLocalFiles(const QStringList& root);
protected:
	DLSiteClient client;
	QHttpServer qserver;
	QTimer daily_timer;
};

