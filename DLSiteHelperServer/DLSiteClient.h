#pragma once
#include "DLConfig.h"
#include <QObject>
#include <QByteArray>
#include <set>
#include <vector>
#include "cpr/cpr.h"
class ICIDMLinkTransmitter2;
class DLSiteClient:public QObject
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
		QString cookie;
		WorkType type = UNKNOWN;//作品的类型
		std::set<std::string> download_ext;//下载的文件(解压前)的格式
		bool ready = false;//获取到所有下载链接并交付给IDM
		std::vector<std::string> urls;
		std::vector<std::string> file_names;//下载的文件的文件名
		QString work_name;//作品名
	};
	DLSiteClient();
	~DLSiteClient();
	//线程不安全，只能从主线程调用
	void StartDownload(const QByteArray& cookies, const QByteArray& user_agent,const QStringList& works);
	void StartRename(const QStringList& files);
	using StateMap = std::map<std::string, DLSiteClient::State >;
signals:
	void signalStartDownload(DLSiteClient::StateMap status);
protected:
	//以防万一直接传值
	void RenameProcess(QStringList local_files);
	void DownloadProcess(cpr::Cookies _cookies,cpr::UserAgent user_agent, QStringList works);
	static std::string format(const char * format, ...);
	static bool RenameFile(const QString & file, const QString & id, const QString & work_name);
	static State TryDownloadWork(std::string id, cpr::Cookies cookie, cpr::UserAgent user_agent);
	static WorkType FindWorkTypeFromWeb(const std::string& page,const std::string&id);
	static void SendTaskToIDM(StateMap status);//必须在主线程运行

	static QString unicodeToString(const QString& str);

	std::atomic<bool> running = false;//不管执行哪个任务都用一个running，因为有共用的成员
};

