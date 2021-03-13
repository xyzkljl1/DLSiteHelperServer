#pragma once
#include "DLConfig.h"
#include <QObject>
#include <QByteArray>
#include <QProcess>
#include <set>
#include <vector>
#include "cpr/cpr.h"
#include "Util.h"
class BaseDownloader;
class DLSiteClient:public QObject
{
	Q_OBJECT
public:

	DLSiteClient();
	~DLSiteClient();
	//线程不安全，只能从主线程调用
	void StartDownload(const QByteArray& cookies, const QByteArray& user_agent,const QStringList& works);
	void StartRename(const QStringList& files);
	static Task TryDownloadWork(std::string id, cpr::Cookies cookie, cpr::UserAgent user_agent, bool only_refresh_cookie);
protected:
	//以防万一直接传值
	void RenameThread(QStringList local_files);
	void DownloadThread(QStringList works,cpr::Cookies cookie,cpr::UserAgent user_agent);
	static bool RenameFile(const QString & file, const QString & id, const QString & work_name);
	static WorkType GetWorkTypeFromWeb(const std::string& page, const std::string&id);	
	static QString unicodeToString(const QString& str);

	void OnDownloadDone();
	void OnDownloadAborted();

	std::atomic<bool> running = false;//不管执行哪个任务都用一个running，因为有共用的成员
	BaseDownloader* downloader=nullptr;
};