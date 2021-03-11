#pragma once
#include "DLConfig.h"
#include <QObject>
#include <QByteArray>
#include <QProcess>
#include <set>
#include <vector>
#include "cpr/cpr.h"
//只会作为std::string用的定义为std::string,其余统统定义为QString
class __declspec(dllexport)  DLSiteClient:public QObject
{
	Q_OBJECT
public:
	enum WorkType {
		UNKNOWN,//尚未获取到
		VIDEO,//视频
		AUDIO,//音频
		PICTURE,//图片
		PROGRAM,//程序
		OTHER,//获取失败/获取到了但不知道是什么
		CANTDOWNLOAD//浏览器专用，无法下载
	};
	struct State {
		std::string cookie;
		WorkType type = UNKNOWN;//作品的类型
		bool ready = false;//获取到所有下载链接并交付给aria2
		std::set<std::string> download_ext;//下载的文件(解压前)的格式
		std::vector<std::string> urls;
		std::vector<std::string> file_names;//下载的文件的文件名
		std::vector<std::string> aria_id;
		QString work_name;//作品名
	};
	DLSiteClient();
	~DLSiteClient();
	//线程不安全，只能从主线程调用
	void StartDownload(const QByteArray& cookies, const QByteArray& user_agent,const QStringList& works);
	void StartRename(const QStringList& files);
	using StateMap = std::map<std::string, DLSiteClient::State>;
protected:
	//以防万一直接传值
	void RenameThread(QStringList local_files);
	void DownloadThread(QStringList works);
	//检查任务状态，如不正常则重新获取cookie并重启任务，is_first_time为真时是第一次执行
	//全部完成时返回true
	bool CheckTaskStatus(bool init);
	//检查Aria2(程序自身)的状态，如不正常则重启，必须在主线程执行
	void CheckAria2Status(bool init);

	static bool RenameFile(const QString & file, const QString & id, const QString & work_name);
	static State TryDownloadWork(std::string id, cpr::Cookies cookie, cpr::UserAgent user_agent, bool only_refresh_cookie);
	static WorkType GetWorkTypeFromWeb(const std::string& page, const std::string&id);
	static std::string format(const char * format, ...);
	static QString unicodeToString(const QString& str);

	QProcess* aria2_process=nullptr;
	std::atomic<bool> running = false;//不管执行哪个任务都用一个running，因为有共用的成员
	StateMap task_map;
	cpr::Cookies main_cookies;
	cpr::UserAgent user_agent;
};