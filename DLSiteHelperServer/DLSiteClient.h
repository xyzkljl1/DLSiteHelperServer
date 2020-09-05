#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QByteArray>
#include <set>
class QNetworkReply;
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
		OTHER//获取失败/获取到了但不知道是什么
	};
	struct State {
		WorkType type = UNKNOWN;//作品的类型
		std::set<std::string> download_ext;//下载的文件(解压前)的格式
		bool ready = false;//获取到所有下载链接并交付给IDM
		std::vector<std::string> urls;
		std::vector<std::string> file_names;//下载的文件的文件名
		QString work_name;//作品名
		bool failed = false;//获取下载地址失败，获取类型失败不算
	};
	DLSiteClient();
	~DLSiteClient();
	//线程不安全，只能从主线程调用
	void StartDownload(const QByteArray& cookies,const std::vector<std::string>& works);
	void StartRename(const QStringList& files);
signals:
	void downloadStatusFinished();
	void renameStatusFinished();
protected:
	static QString unicodeToString(const QString& str);
	void onReceiveType(QNetworkReply*, std::string id);
	void onReceiveProductInfo(QNetworkReply*, std::string id);
	void onReceiveDownloadTry(QNetworkReply*, std::string id, int idx);
	void onDownloadStatusFinished();
	void onRenameStatusFinished();
	void SendTaskToIDM();

	QNetworkAccessManager manager;
	QByteArray cookies;
	bool running = false;//不管执行哪个任务都用一个running，因为有共用的成员
	std::map<std::string, State> status;
	QStringList local_files;
	const std::string DOWNLOAD_DIR = "D:/IDMDownload/AutoDownload/";
};

