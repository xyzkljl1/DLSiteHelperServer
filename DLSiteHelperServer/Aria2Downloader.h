#include "BaseDownloader.h"
import Util;
import DLConfig;
using namespace Util;
class QProcess;
//线程不安全
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
	//检查任务状态，如不正常则重新获取cookie并重启任务，is_first_time为真时是第一次执行
	//全部完成或无法继续时返回true
	bool CheckTaskStatus(bool init);
	//检查Aria2(程序自身)的状态，如不正常则重启，必须在主线程执行
	void CheckAria2Status(bool init);

	std::vector<Aria2Task> task_list;
	QProcess* aria2_process=nullptr;
	cpr::Cookies main_cookie;
	cpr::UserAgent user_agent;
	std::atomic_bool running = false;
};

