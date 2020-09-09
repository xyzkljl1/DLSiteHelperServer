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
		OTHER,//��ȡʧ��/��ȡ���˵���֪����ʲô
		CANTDOWNLOAD//�����ר�ã��޷�����
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
	void StartDownload(const QByteArray& cookies,const QStringList& works);
	void StartRename(const QStringList& files);
signals:
	void downloadStatusChanged();
	void renameStatusChanged();
protected:
	static QString unicodeToString(const QString& str);
	void onReceiveType(QNetworkReply*, QString id);
	void onReceiveProductInfo(QNetworkReply*, QString id);
	void onReceiveDownloadTry(QNetworkReply*, QString id, int idx);
	void onDownloadStatusChanged();
	void onRenameStatusChanged();
	void SendTaskToIDM();

	QNetworkAccessManager manager;
	QByteArray cookies;
	bool running = false;//����ִ���ĸ�������һ��running����Ϊ�й��õĳ�Ա
	std::map<QString, State> status;
	QStringList local_files;
	const std::string DOWNLOAD_DIR = "D:/IDMDownload/AutoDownload/";
};

