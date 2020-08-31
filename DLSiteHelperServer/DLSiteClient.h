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
		UNKNOWN,//��δ��ȡ��
		VIDEO,//��Ƶ
		AUDIO,//��Ƶ
		PICTURE,//ͼƬ
		PROGRAM,//����
		OTHER//��ȡʧ��/��ȡ���˵���֪����ʲô
	};
	struct State {
		WorkType type = UNKNOWN;
		std::set<std::string> download_ext;//���ص��ļ�(��ѹǰ)�ĸ�ʽ
		bool ready = false;//��ȡ�������������Ӳ�������IDM
		std::vector<std::string> urls;
		std::vector<std::string> file_names;
		bool failed = false;//��ȡ���ص�ַʧ�ܣ���ȡ����ʧ�ܲ���
	};
	DLSiteClient();
	~DLSiteClient();
	//�̲߳���ȫ��ֻ�ܴ����̵߳���
	void StartDownload(const QByteArray& cookies,const std::vector<std::string>& works);
signals:
	void statusFinished();
protected:
	void onReceiveType(QNetworkReply*, std::string id);
	void onReceiveDownloadTry(QNetworkReply*, std::string id, int idx);
	void onStatusFinished();
	void SendTaskToIDM();

	QNetworkAccessManager manager;
	QByteArray cookies;
	bool running=false;
	std::map<std::string, State> status;
	const std::string DOWNLOAD_DIR = "D:/IDMDownload/AutoDownload/";
};

