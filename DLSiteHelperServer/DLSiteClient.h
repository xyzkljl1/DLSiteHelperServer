#pragma once
#include "DLConfig.h"
#include <QObject>
#include <QByteArray>
#include <QProcess>
#include <set>
#include <vector>
#include "cpr/cpr.h"
//ֻ����Ϊstd::string�õĶ���Ϊstd::string,����ͳͳ����ΪQString
class __declspec(dllexport)  DLSiteClient:public QObject
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
		std::string cookie;
		WorkType type = UNKNOWN;//��Ʒ������
		bool ready = false;//��ȡ�������������Ӳ�������aria2
		std::set<std::string> download_ext;//���ص��ļ�(��ѹǰ)�ĸ�ʽ
		std::vector<std::string> urls;
		std::vector<std::string> file_names;//���ص��ļ����ļ���
		std::vector<std::string> aria_id;
		QString work_name;//��Ʒ��
	};
	DLSiteClient();
	~DLSiteClient();
	//�̲߳���ȫ��ֻ�ܴ����̵߳���
	void StartDownload(const QByteArray& cookies, const QByteArray& user_agent,const QStringList& works);
	void StartRename(const QStringList& files);
	using StateMap = std::map<std::string, DLSiteClient::State>;
protected:
	//�Է���һֱ�Ӵ�ֵ
	void RenameThread(QStringList local_files);
	void DownloadThread(QStringList works);
	//�������״̬���粻���������»�ȡcookie����������is_first_timeΪ��ʱ�ǵ�һ��ִ��
	//ȫ�����ʱ����true
	bool CheckTaskStatus(bool init);
	//���Aria2(��������)��״̬���粻���������������������߳�ִ��
	void CheckAria2Status(bool init);

	static bool RenameFile(const QString & file, const QString & id, const QString & work_name);
	static State TryDownloadWork(std::string id, cpr::Cookies cookie, cpr::UserAgent user_agent, bool only_refresh_cookie);
	static WorkType GetWorkTypeFromWeb(const std::string& page, const std::string&id);
	static std::string format(const char * format, ...);
	static QString unicodeToString(const QString& str);

	QProcess* aria2_process=nullptr;
	std::atomic<bool> running = false;//����ִ���ĸ�������һ��running����Ϊ�й��õĳ�Ա
	StateMap task_map;
	cpr::Cookies main_cookies;
	cpr::UserAgent user_agent;
};