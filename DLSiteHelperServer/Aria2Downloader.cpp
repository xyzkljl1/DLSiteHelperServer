#include "Aria2Downloader.h"
#include <Windows.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <ShellAPI.h>
#include <QTimer>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
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
	//��ʼ������
	if (CheckTaskStatus(true))
	{
		running = false;
		return false;
	}
	//��ʼ���ڼ��״̬
	std::thread thread(&Aria2Downloader::CheckThread, this);
	thread.detach();
	return true;
}
void Aria2Downloader::CheckThread()
{
	do {
		Sleep(1000 * 30);
	} while (!CheckTaskStatus(false));
	//Aria2�����ڱ��ش���Ŀ���ļ�������Ҫ�ȼ������״̬�ټ�鱾���ļ�
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
	running = false;
	emit signalDownloadDone();
}
void Aria2Downloader::CheckAria2Status(bool init) {
	if (init)
	{
		//�ر�֮ǰ��aria2
		TCHAR tszProcess[64] = { 0 };
		lstrcpy(tszProcess, _T("aria2c(DLSite).exe"));
		//���ҽ���
		STARTUPINFO st;
		PROCESS_INFORMATION pi;
		PROCESSENTRY32 ps;
		HANDLE hSnapshot;
		memset(&st, 0, sizeof(STARTUPINFO));
		st.cb = sizeof(STARTUPINFO);
		memset(&ps, 0, sizeof(PROCESSENTRY32));
		ps.dwSize = sizeof(PROCESSENTRY32);
		memset(&pi, 0, sizeof(PROCESS_INFORMATION));
		// �������� 
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
	//file-allocation=fallocʱ����Ȩ
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
		aria2_process->setProgram("\"" + QCoreApplication::applicationDirPath() + "/aria2/aria2c(DLSite).exe\"");//������/�ո��·����Ҫ������������
		aria2_process->setArguments({ "--conf-path=aria2.conf","--all-proxy=" + DLConfig::ARIA2_PROXY,
									QString("--rpc-listen-port=%1").arg(DLConfig::ARIA2_PORT),/*"--referer=\"https://www.dlsite.com/\"","--check-certificate=false"*/ });
#ifdef _DEBUG
		//��ʾ����
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
	if (init)//����Ŀ��Ŀ¼����ֹ������ļ�Ӱ������
		QDir(DLConfig::DOWNLOAD_DIR).removeRecursively();
	/*
	aria2�г���������޷�������ֻ�ܴ���һ���µ������(���Զ��ϵ�����)
	�������̫�ർ�¾ɵ������ͷţ����ܻ����ѯ�ò�����ȷ�Ľ��
	���ǵ��������غܶ��ļ����������ļ��н�max-download-result������ܱ����������
	*/
	cpr::Session session;
	session.SetUrl(cpr::Url{ QString("http://127.0.0.1:%1/jsonrpc").arg(DLConfig::ARIA2_PORT).toStdString() });

	std::vector<Aria2Task> tmp_task_list;
	float size_ct = .0f;
	float total_size_ct = .0f;
	//aria2�������
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
		//�ҵ���Ҫ���µ�������
		total_ct += task.urls.size();
		std::vector<int> update_index;//��Ҫ���µ������������
		for (int i = 0; i < task.aria2_id.size(); ++i)//aria_id��url��sizeӦ����һ�µ�
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
					printf("Query Aria2 Error %s on %s\n", response.text.c_str(), task.urls[i].c_str());
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
		if (!init)//������ǵ�һ�δ�������ʧЧ���������������cookie���ڣ���Ҫ���»�ȡ
			task.cookie = DLSiteClient::TryDownloadWork(task.id, main_cookie, user_agent, true).cookie;
		//����������
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
					task.cookie.c_str());
			session.SetBody(cpr::Body{ data.c_str() });
			auto response = session.Post();
			if (response.status_code != 200)
			{
				printf("Update Aria2 Error %s on %s\n", response.text.c_str(), task.urls[i].c_str());
				continue;
			}
			QJsonParseError jsonError;
			QJsonDocument doc = QJsonDocument::fromJson(response.text.c_str(), &jsonError);
			if (jsonError.error != QJsonParseError::NoError)
			{
				printf("Update Aria2 Error %s on %s\n", response.text.c_str(), task.urls[i].c_str());
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
	if (init)//ȥ���޷�/����Ҫ���ص�����
	{
		printf("Start Download %d/%d works\n", (int)tmp_task_list.size(), (int)task_list.size());
		task_list = tmp_task_list;
	}
	printf("%d(Running)/%d(Updating)/%d(Done)/%d(Unknown) in %d files. %d/%d updated. %6.2f/%6.2fMB downloaded.\n",
		running_ct, update_ct, complete_ct, unknown_ct, total_ct, update_success_ct, update_ct, size_ct, total_size_ct);
	return complete_ct == total_ct;
}
