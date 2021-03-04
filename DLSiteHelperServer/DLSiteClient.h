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
		UNKNOWN,//��δ��ȡ��
		VIDEO,//��Ƶ
		AUDIO,//��Ƶ
		PICTURE,//ͼƬ
		PROGRAM,//����
		OTHER,//��ȡʧ��/��ȡ���˵���֪����ʲô
		CANTDOWNLOAD//�����ר�ã��޷�����
	};
	struct State {
		QString cookie;
		WorkType type = UNKNOWN;//��Ʒ������
		std::set<std::string> download_ext;//���ص��ļ�(��ѹǰ)�ĸ�ʽ
		bool ready = false;//��ȡ�������������Ӳ�������IDM
		std::vector<std::string> urls;
		std::vector<std::string> file_names;//���ص��ļ����ļ���
		QString work_name;//��Ʒ��
	};
	DLSiteClient();
	~DLSiteClient();
	//�̲߳���ȫ��ֻ�ܴ����̵߳���
	void StartDownload(const QByteArray& cookies, const QByteArray& user_agent,const QStringList& works);
	void StartRename(const QStringList& files);
	using StateMap = std::map<std::string, DLSiteClient::State >;
signals:
	void signalStartDownload(DLSiteClient::StateMap status);
protected:
	//�Է���һֱ�Ӵ�ֵ
	void RenameProcess(QStringList local_files);
	void DownloadProcess(cpr::Cookies _cookies,cpr::UserAgent user_agent, QStringList works);
	static std::string format(const char * format, ...);
	static bool RenameFile(const QString & file, const QString & id, const QString & work_name);
	static State TryDownloadWork(std::string id, cpr::Cookies cookie, cpr::UserAgent user_agent);
	static WorkType FindWorkTypeFromWeb(const std::string& page,const std::string&id);
	static void SendTaskToIDM(StateMap status);//���������߳�����

	static QString unicodeToString(const QString& str);

	std::atomic<bool> running = false;//����ִ���ĸ�������һ��running����Ϊ�й��õĳ�Ա
};

