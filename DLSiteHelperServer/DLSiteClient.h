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
		WorkType type = UNKNOWN;//��Ʒ������
		std::set<std::string> download_ext;//���ص��ļ�(��ѹǰ)�ĸ�ʽ
		bool ready = false;//��ȡ�������������Ӳ�������IDM
		std::vector<std::string> urls;
		std::vector<std::string> file_names;//���ص��ļ����ļ���
		QString work_name;//��Ʒ��
		bool failed = false;//��ȡ���ص�ַʧ�ܣ���ȡ����ʧ�ܲ���
	};
	DLSiteClient();
	~DLSiteClient();
	//�̲߳���ȫ��ֻ�ܴ����̵߳���
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
	bool running = false;//����ִ���ĸ�������һ��running����Ϊ�й��õĳ�Ա
	std::map<std::string, State> status;
	QStringList local_files;
	const std::string DOWNLOAD_DIR = "D:/IDMDownload/AutoDownload/";
};

