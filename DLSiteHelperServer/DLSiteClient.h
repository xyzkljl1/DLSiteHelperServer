#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QByteArray>
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
		WorkType type = UNKNOWN;
		bool ready = false;//获取到所有下载链接并交付给IDM
		std::vector<std::string> urls;
		bool failed = false;//获取下载地址失败，获取类型失败不算
	};
	DLSiteClient();
	~DLSiteClient();
	//线程不安全，只能从主线程调用
	void start(const QByteArray& cookies,const std::vector<std::string>& works);
	void onReceiveType(QNetworkReply*, std::string id);
	void onReceiveDownloadTry(QNetworkReply*, std::string id,int idx);
	void onStatusFinished();
signals:
	void statusFinished();
protected:
	QNetworkAccessManager manager;
	QByteArray cookies;
	bool running=false;
	std::map<std::string, State> status;
};

