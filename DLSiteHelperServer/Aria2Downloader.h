#pragma once
#include "BaseDownloader.h"
class QProcess;
//�̲߳���ȫ
class Aria2Downloader:public BaseDownloader
{
	Q_OBJECT
public:
	struct Aria2Task :public Task {
		std::vector<bool>        aria2_success;
		std::vector<std::string> aria2_id;
		Aria2Task(const Task& t) :Task(t) {
			aria2_id.resize(t.urls.size(), "");
			aria2_success.resize(t.urls.size(), false);
		}
	};
	Aria2Downloader();
	~Aria2Downloader();
	bool StartDownload(const std::vector<Task>& _tasks, const cpr::Cookies& _cookie, const cpr::UserAgent& _user_agent) override;
protected:
	void CheckThread();
	//�������״̬���粻���������»�ȡcookie����������is_first_timeΪ��ʱ�ǵ�һ��ִ��
	//ȫ����ɻ��޷�����ʱ����true
	bool CheckTaskStatus(bool init);
	//���Aria2(��������)��״̬���粻���������������������߳�ִ��
	void CheckAria2Status(bool init);

	std::vector<Aria2Task> task_list;
	QProcess* aria2_process=nullptr;
	cpr::Cookies main_cookie;
	cpr::UserAgent user_agent;
	std::atomic_bool running = false;
};

