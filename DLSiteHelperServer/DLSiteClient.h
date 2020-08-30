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
		VIDEO,//��Ƶ
		AUDIO,//��Ƶ
		PICTURE,//ͼƬ
		PROGRAM,//����
	};
	struct State {
		int type = 0;
	};
	DLSiteClient();
	~DLSiteClient();
	//�̲߳���ȫ��ֻ�ܴ����̵߳���
	void start(const QByteArray& cookies,const std::vector<std::string>& works);
	void onReceive(QNetworkReply*);
protected:
	QNetworkAccessManager manager;
	QByteArray cookies;
	bool running=false;
};

