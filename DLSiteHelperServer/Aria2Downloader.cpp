#include "Aria2Downloader.h"
#include <Windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <ShellAPI.h>
#include <QTimer>
#include <QDir>
#include <QCoreApplication>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
import DLSiteClientUtil;
Aria2Downloader::Aria2Downloader() {
	auto timer = new QTimer(this);
	timer->setSingleShot(false);
	timer->setInterval(1000 * 60 * 60);
	connect(timer, &QTimer::timeout, this, std::bind(&Aria2Downloader::CheckAria2Status, this, false));
	CheckAria2Status(true);
}
Aria2Downloader::~Aria2Downloader()
{
	if (aria2_process)
		aria2_process->kill();		
}
bool Aria2Downloader::StartDownload(const std::vector<Task>& _tasks, const cpr::Cookies& _cookie, const cpr::UserAgent& _user_agent)
{
	if (running)
		return false;
	running = true;
	main_cookie = _cookie;
	user_agent = _user_agent;
	task_list.clear();
	for (auto& _task : _tasks)
		task_list.push_back(_task);
	//初始化任务
	if (CheckTaskStatus(true)&&!task_list.empty())
	{
		running = false;
		return false;
	}
	//开始定期检查状态
	std::thread thread(&Aria2Downloader::CheckThread, this);
	thread.detach();
	return true;
}
void Aria2Downloader::CheckThread()
{
	do
#ifdef _DEBUG
		Sleep(1000 * 30);
#else
		//我的网速快如魔鬼，不需要把间隔设得很长
		Sleep(1000 * 15 * 30);
#endif
	while (!CheckTaskStatus(false));
	//Aria2会先在本地创建目标文件，所以要先检查任务状态再检查本地文件
	std::vector<QString> files;
	for (auto&task : task_list)
		for (int i = 0; i < task.urls.size(); ++i)
			if (!task.aria2_success[i])
			{
				running = false;
				emit signalDownloadAborted();
				return;
			}
			else
				files.push_back(QString::fromLocal8Bit(task.GetDownloadPath(i).c_str()));
	if(!CheckFiles(files))
	{
		running = false;
		emit signalDownloadAborted();
		return;
	}
	std::vector<Task> tmp;
	for (auto task : task_list)
		tmp.push_back(task);
	running = false;
	emit signalDownloadDone(tmp);
}
void Aria2Downloader::CheckAria2Status(bool init) {
	if (init)
	{
		//关闭之前的aria2
		TCHAR tszProcess[64] = { 0 };
		lstrcpy(tszProcess, _T("aria2c(DLSite).exe"));
		//查找进程
		STARTUPINFO st;
		PROCESS_INFORMATION pi;
		PROCESSENTRY32 ps;
		HANDLE hSnapshot;
		memset(&st, 0, sizeof(STARTUPINFO));
		st.cb = sizeof(STARTUPINFO);
		memset(&ps, 0, sizeof(PROCESSENTRY32));
		ps.dwSize = sizeof(PROCESSENTRY32);
		memset(&pi, 0, sizeof(PROCESS_INFORMATION));
		// 遍历进程 
		hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot != INVALID_HANDLE_VALUE && Process32First(hSnapshot, &ps))
			do {
				if (lstrcmp(ps.szExeFile, tszProcess) == 0)
				{
					HANDLE killHandle = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION |   // Required by Alpha
						PROCESS_CREATE_THREAD |   // For CreateRemoteThread
						PROCESS_VM_OPERATION |   // For VirtualAllocEx/VirtualFreeEx
						PROCESS_VM_WRITE,             // For WriteProcessMemory);
						FALSE, ps.th32ProcessID);
					if (killHandle)
						TerminateProcess(killHandle, 0);
				}
			} while (Process32Next(hSnapshot, &ps));
			CloseHandle(hSnapshot);
	}
	//file-allocation=falloc时需提权
	if ((!aria2_process) || aria2_process->state() != QProcess::Running)
	{
		if (aria2_process)
		{
			aria2_process->close();
			aria2_process->waitForFinished();
			delete aria2_process;
		}
		auto dir = QCoreApplication::applicationDirPath();
		aria2_process = new QProcess(this);
		aria2_process->setWorkingDirectory(QCoreApplication::applicationDirPath() + "/aria2");
		aria2_process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
		aria2_process->setProgram("\"" + QCoreApplication::applicationDirPath() + "/aria2/aria2c(DLSite).exe\"");//有括号/空格的路径需要用引号括起来
		aria2_process->setArguments({ "--conf-path=aria2.conf","--all-proxy=" + DLConfig::ARIA2_PROXY,
									QString("--rpc-listen-port=%1").arg(DLConfig::ARIA2_PORT),/*"--referer=\"https://www.dlsite.com/\"","--check-certificate=false"*/ });
#ifdef _DEBUG
		//显示窗口
		aria2_process->setCreateProcessArgumentsModifier(
			[](QProcess::CreateProcessArguments * args) {
			args->flags |= CREATE_NEW_CONSOLE;
			args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
		});
#endif
		aria2_process->start();
		aria2_process->waitForStarted();
	}
}
bool Aria2Downloader::CheckTaskStatus(bool init)
{
	if (init)//清理目标目录，防止错误的文件影响下载
		QDir(DLConfig::DOWNLOAD_DIR).removeRecursively();
	/*
	aria2中出错的任务无法继续，只能创建一个新的替代它(会自动断点续传)
	如果任务太多导致旧的任务被释放，可能会令查询得不到正确的结果
	考虑到不会下载很多文件，在配置文件中将max-download-result调大就能避免这个问题
	*/
	cpr::Session session;
	session.SetUrl(cpr::Url{ QString("http://127.0.0.1:%1/jsonrpc").arg(DLConfig::ARIA2_PORT).toStdString() });

	std::vector<Aria2Task> tmp_task_list;
	float size_ct = .0f;
	float total_size_ct = .0f;
	//aria2任务计数
	int unknown_ct = 0;
	int update_ct = 0;
	int running_ct = 0;
	int complete_ct = 0;
	int total_ct = 0;
	int update_success_ct = 0;
	for (auto& task : task_list)
	{
		if (task.type == WorkType::CANTDOWNLOAD)
			continue;
		if (!task.ready)
			continue;
		if (task.urls.empty())
			continue;
		//找到需要更新的子任务
		total_ct += task.urls.size();
		std::vector<int> update_index;//需要更新的子任务的索引
		for (int i = 0; i < task.aria2_id.size(); ++i)//aria_id和url的size应该是一致的
			if (task.aria2_id[i] == "")
				update_index.push_back(i);
			else
			{
				std::string data =
					Format("{\"jsonrpc\": \"2.0\",\"id\":\"DLSite\",\"method\": \"aria2.tellStatus\","
						"\"params\": [\"token:%s\",\"%s\"]}",
						DLConfig::ARIA2_SECRET.c_str(),
						task.aria2_id[i].c_str());
				session.SetBody(cpr::Body{ data.c_str() });
				auto response = session.Post();
				if (response.status_code != 200)
				{
					LogError("Query Aria2 Error %s on %s\n", response.text.c_str(), task.urls[i].c_str());
					unknown_ct++;
					continue;
				}
				QJsonParseError error;
				QJsonDocument doc = QJsonDocument::fromJson(response.text.c_str(), &error);
				if (error.error != QJsonParseError::NoError || !doc.object().contains("result"))
				{
					unknown_ct++;
					continue;
				}
				auto result = doc.object().value("result").toObject();
				QString task_status = result.value("status").toString();
				size_ct += result.value("completedLength").toString().toInt() / 1024.0f / 1024.0f;
				total_size_ct += result.value("totalLength").toString().toInt() / 1024.0f / 1024.0f;
				//active,waiting,paused,error,complete,removed
				if (task_status == "error" || task_status == "paused" || task_status == "removed")
					update_index.push_back(i);
				else if (task_status == "complete")
				{
					task.aria2_success[i] = true;
					complete_ct++;
				}
				else
					running_ct++;
			}
		update_ct += update_index.size();
		if (update_index.empty())
			continue;
		if (!init)//如果不是第一次创建任务，失效的任务可能是由于cookie过期，需要重新获取
			task.cookies = DLSiteClientUtil::MakeDownloadTask(task.id, main_cookie, user_agent).cookies;
		//更新子任务
		std::string path = task.GetDownloadDir();
		if (!QDir(QString::fromLocal8Bit(path.c_str())).exists())
			QDir().mkpath(QString::fromLocal8Bit(path.c_str()));
		for (auto& i : update_index)
		{
			std::string data =
				Format("{\"jsonrpc\": \"2.0\",\"id\":\"DLSite\",\"method\": \"aria2.addUri\","
					"\"params\": [\"token:%s\",[\"%s\"],{\"dir\":\"%s\",\"out\":\"%s\""
					",\"header\":[\"User-Agent: %s\",\"Cookie: %s\"]"
					"}]}",
					DLConfig::ARIA2_SECRET.c_str(),
					task.urls[i].c_str(),
					path.c_str(),
					task.file_names[i].c_str(),
					user_agent.c_str(),
					task.cookies[i].c_str());
			session.SetBody(cpr::Body{ data.c_str() });
			auto response = session.Post();
			if (response.status_code != 200)
			{
				LogError("Update Aria2 Error %s on %s\n", response.text.c_str(), task.urls[i].c_str());
				continue;
			}
			QJsonParseError jsonError;
			QJsonDocument doc = QJsonDocument::fromJson(response.text.c_str(), &jsonError);
			if (jsonError.error != QJsonParseError::NoError)
			{
				LogError("Update Aria2 Error %s on %s\n", response.text.c_str(), task.urls[i].c_str());
				continue;
			}
			auto root = doc.object();
			if (root.contains("result"))
			{
				task.aria2_id[i] = root.value("result").toString().toStdString();
				update_success_ct++;
			}
		}
		if (init)
			tmp_task_list.push_back(task);
	}
	if (init)//去掉无法/不需要下载的任务
	{
		Log("Start Download %d/%d works\n", (int)tmp_task_list.size(), (int)task_list.size());
		task_list = tmp_task_list;
	}
	Log("%d(Running)/%d(Updating)/%d(Done)/%d(Unknown) in %d files. %d/%d updated. %6.2f/%6.2fMB downloaded.\n",
		running_ct, update_ct, complete_ct, unknown_ct, total_ct, update_success_ct, update_ct, size_ct, total_size_ct);
	return complete_ct == total_ct;
}
