#pragma once
#include <QObject>
#include <QByteArray>
#include <QProcess>
#include <QStringList>
#include <set>
#include <vector>
#include "cpr/cpr.h"
import Util;
/*
* 把继承自QObject的类改成ixx时，即使把定义放到.h文件里并声称moc文件，也无法正常生成metacall/signals等函数体，目前不知道如何解决
*/
class BaseDownloader;
class DLSiteClient:public QObject
{
	Q_OBJECT
public:
	struct WorkInfo {
		QString work_info_text;//info原文
		QStringList translations;//多语言版本
		bool is_otm=false;//乙女向
	};
	DLSiteClient();
	~DLSiteClient();
	//线程不安全，只能从主线程调用
	void StartDownload(const QByteArray& cookies, const QByteArray& user_agent,const QStringList& works);
	void StartRename(const QStringList& files);

	//返回workInfo和多语言版本
	WorkInfo FetchWorksInfo(const QString& id);
protected:
	//以防万一直接传值
	void RenameThread(QStringList local_files);
	void DownloadThread(QStringList works,cpr::Cookies cookie,cpr::UserAgent user_agent);
	static bool RenameFile(const QString & file, const QString & id, const QString & work_name);

	//返回response和派生为JsonObject的response
	static QPair<QString, QJsonObject> GetWorkInfoFromDLSiteAPI(cpr::Session& session, const QString& id);
	static WorkType GetWorkTypeFromWeb(const std::string& page, const std::string&id);	
	static QString unicodeToString(const QString& str);
	bool Extract(const QString& file_name,const QString& dir);

	void OnDownloadDone(std::vector<Task> task_list);
	void OnDownloadAborted();

	std::atomic<bool> running = false;//不管执行哪个任务都用一个running，因为有共用的成员
	BaseDownloader* downloader=nullptr;
};