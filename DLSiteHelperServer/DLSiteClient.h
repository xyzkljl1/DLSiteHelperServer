#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QByteArray>
class QNetworkReply;
class DLSiteClient:public QObject
{
	Q_OBJECT
public:
	enum Type {
		UNDEFINED,
		VIDEO,//视频
		AUDIO,//音频
		PICTURE,//图片
		PROGRAM,//程序
	};
	struct State {
		int type = 0;
	};
	DLSiteClient();
	~DLSiteClient();
	//线程不安全，只能从主线程调用
	void start(const QByteArray& cookies,const std::vector<std::string>& works);
	void onReceive(QNetworkReply*);
protected:
	QNetworkAccessManager manager;
	QByteArray cookies;
	bool running=false;
};

