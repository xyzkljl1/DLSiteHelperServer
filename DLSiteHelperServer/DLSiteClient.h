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
	//�̲߳���ȫ��ֻ�ܴ����̵߳���
	void StartDownload(const QByteArray& cookies, const QByteArray& user_agent,const QStringList& works);
	void StartRename(const QStringList& files);

	QStringList GetTranslationWorks(const QString& id);
	QStringList GetOTMWorks(const QStringList& works);
	static Task TryDownloadWork(std::string id, cpr::Cookies cookie, cpr::UserAgent user_agent, bool only_refresh_cookie);
protected:
	//�Է���һֱ�Ӵ�ֵ
	void RenameThread(QStringList local_files);
	void DownloadThread(QStringList works,cpr::Cookies cookie,cpr::UserAgent user_agent);
	static bool RenameFile(const QString & file, const QString & id, const QString & work_name);

	static QJsonObject GetWorkInfoFromDLSiteAPI(cpr::Session& session,const QString& id);
	static WorkType GetWorkTypeFromWeb(const std::string& page, const std::string&id);	
	static QString unicodeToString(const QString& str);
	bool Extract(const QString& file_name,const QString& dir);

	void OnDownloadDone(std::vector<Task> task_list);
	void OnDownloadAborted();

	std::atomic<bool> running = false;//����ִ���ĸ�������һ��running����Ϊ�й��õĳ�Ա
	BaseDownloader* downloader=nullptr;
};