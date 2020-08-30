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
		UNKNOWN,//��δ��ȡ��
		VIDEO,//��Ƶ
		AUDIO,//��Ƶ
		PICTURE,//ͼƬ
		PROGRAM,//����
		OTHER//��ȡʧ��/��ȡ���˵���֪����ʲô
	};
	struct State {
		WorkType type = UNKNOWN;
		bool ready = false;//��ȡ�������������Ӳ�������IDM
		std::vector<std::string> urls;
		bool failed = false;//��ȡ���ص�ַʧ�ܣ���ȡ����ʧ�ܲ���
	};
	DLSiteClient();
	~DLSiteClient();
	//�̲߳���ȫ��ֻ�ܴ����̵߳���
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

