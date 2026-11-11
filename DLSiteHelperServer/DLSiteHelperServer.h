#pragma once
import DLConfig;
#include "DLSiteClient.h"
#include <QStringList>
#include <QTimer>
#include <QRegExp>
#include <QtHttpServer/QHttpServer>
#include <set>
using namespace Util;
class DLSiteHelperServer:public QObject
{
	Q_OBJECT
public:
	DLSiteHelperServer(QObject* parent);
	~DLSiteHelperServer();
protected:
	void HandleRequest(const QHttpServerRequest& request, QHttpServerResponder&& responseresponse);
	void ReplyText(QHttpServerResponder&& responder, const QHttpServerResponder::StatusCode& status = QHttpServerResponder::StatusCode::Ok, const QString& message = "Success");
	
	[[nodiscard]] QString GetAllInvalidWorksString();
	[[nodiscard]] QString GetAllOverlapWorksString();
	QString UpdateBoughtItems(const QByteArray& data);

	[[nodiscard]] std::map<std::string, std::set<std::string>> GetAllOverlapWorks();
	[[nodiscard]] QStringList GetDownloadWorks();
	void DailyTask();
	void SyncLocalFileToDB();
	void FetchWorkInfo(int limit=100);//从DLSite获取workinfo并存入数据库
	void DownloadAll(const QByteArray& cookie, const QByteArray& user_agent);
	void RenameLocal();
	struct LocalScanDir
	{
		std::filesystem::path path;
		bool rj_only = false;
	};
	[[nodiscard]] std::vector<LocalScanDir> GetLocalScanDirs(const std::vector<std::string>& roots);
	[[nodiscard]] std::vector<std::filesystem::path> GetLocalFiles(const LocalScanDir& root);
protected:
	constexpr static int SQL_LENGTH_LIMIT= 10000;
	DLSiteClient client;
	QHttpServer qserver;
	QTimer daily_timer;
};

