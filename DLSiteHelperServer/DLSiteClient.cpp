#include "DLSiteClient.h"
#include "DBProxyServer.h"
#include <regex>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include "cpr/cpr.h"
Q_DECLARE_METATYPE(DLSiteClient::StateMap);//ɳ����ϲ��������ţ����Ա����������
DLSiteClient::DLSiteClient()
{
	qRegisterMetaType<DLSiteClient::StateMap>();
	auto timer = new QTimer(this);
	timer->setSingleShot(false);
	timer->setInterval(1000 * 60 * 60);
	connect(timer, &QTimer::timeout, this, std::bind(&DLSiteClient::CheckAria2Status,this,false));
	CheckAria2Status(true);
}

DLSiteClient::~DLSiteClient()
{
}
void DLSiteClient::CheckAria2Status(bool init) {
	if (init)
	{
		//TODO:�ر�֮ǰ��aria2
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
bool DLSiteClient::CheckTaskStatus(bool init)
{
	if (init)//����Ŀ��Ŀ¼����ֹ������ļ�Ӱ������
		QDir(DLConfig::DOWNLOAD_DIR).removeRecursively();		
	/*
	aria2�г���������޷�������ֻ�ܴ���һ���µ������(���Զ��ϵ�����)
	�������̫�ർ�¾ɵ������ͷţ����ܻ����ѯ�ò�����ȷ�Ľ��
	���ǵ��������غܶ��ļ����������ļ��н�max-download-result������ܱ����������
	*/
	cpr::Session session;
	session.SetUrl(cpr::Url{ QString("http://127.0.0.1:%1/jsonrpc").arg(DLConfig::ARIA2_PORT).toStdString()});

	StateMap tmp_task_map;
	//aria2�������
	int unknown_ct = 0;
	int update_ct = 0;
	int running_ct = 0;
	int complete_ct = 0;
	int total_ct = 0;
	int update_success_ct = 0;
	for (auto& task_pair : task_map)
	{
		auto&task = task_pair.second;
		if (task.type == WorkType::CANTDOWNLOAD)
			continue;
		if (!task.ready)
			continue;
		if (task.urls.empty())
			continue;
		//�ҵ���Ҫ���µ�������
		total_ct += task.urls.size();
		std::vector<int> update_index;//��Ҫ���µ������������
		for (int i = 0; i < task.aria_id.size(); ++i)//aria_id��url��sizeӦ����һ�µ�
			if (task.aria_id[i] == "")
				update_index.push_back(i);
			else
			{
				std::string data =
					format("{\"jsonrpc\": \"2.0\",\"id\":\"DLSite\",\"method\": \"aria2.tellStatus\","
							"\"params\": [\"token:%s\",\"%s\"]}",
							DLConfig::ARIA2_SECRET.c_str(),
							task.aria_id[i].c_str());
				session.SetBody(cpr::Body{ data.c_str() });
				auto response = session.Post();
				if (response.status_code != 200)
				{
					printf("Query Aria2 Error %s on %s\n", response.text.c_str(),task.urls[i].c_str());
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
	//			printf("Status:%s %f\n", task_status.toStdString().c_str(), result.value("completedLength").toString().toInt() / 1024.0f);
				//active,waiting,paused,error,complete,removed
				if (task_status == "error" || task_status == "paused" || task_status == "removed")
					update_index.push_back(i);
				else if (task_status == "complete")
					complete_ct++;
				else
					running_ct++;
			}
		update_ct += update_index.size();
		if (update_index.empty())
			continue;
		if (!init)//������ǵ�һ�δ�������ʧЧ���������������cookie���ڣ���Ҫ���»�ȡ
			task.cookie=TryDownloadWork(task_pair.first, main_cookies, cpr::UserAgent{ user_agent.c_str() }, true).cookie;
		//����������
		std::string path = DLConfig::DOWNLOAD_DIR.toLocal8Bit().toStdString();
		switch (task.type) {
		case WorkType::AUDIO:path += "ASMR/"; break;
		case WorkType::VIDEO:path += "Video/"; break;
		case WorkType::PICTURE:path += "CG/"; break;
		case WorkType::PROGRAM:path += "Game/"; break;
		default:path += "Default/";
		}
		//zip�Ľ�ѹ�껹��Ҫת�룬����Ҫ��rar���ֿ�
		if (task.download_ext.count("zip"))
			path += "zip/";
		else if (task.download_ext.count("rar"))
			path += "rar/";
		else
			path += "other/";
		if (!QDir(QString::fromLocal8Bit(path.c_str())).exists())
			QDir().mkpath(QString::fromLocal8Bit(path.c_str()));
		for (auto& i:update_index)
		{
			 std::string data =
				format("{\"jsonrpc\": \"2.0\",\"id\":\"DLSite\",\"method\": \"aria2.addUri\","
					"\"params\": [\"token:%s\",[\"%s\"],{\"dir\":\"%s\",\"out\":\"%s\""
					",\"header\":[\"User-Agent: %s\",\"Cookie: %s\"]"
					"}]}",
						DLConfig::ARIA2_SECRET.c_str(), 
						task.urls[i].c_str(), 
						path.c_str(), 
						task.file_names[i].c_str(), 
						user_agent.c_str(),
						task.cookie.c_str());
			 session.SetBody(cpr::Body{data.c_str()});
			 auto response =session.Post();
			 if (response.status_code != 200)
			 {
				 printf("Update Aria2 Error %s on %s\n", response.text.c_str(), task.urls[i].c_str());
				 continue;
			 }
			 QJsonParseError jsonError;
			 QJsonDocument doc=QJsonDocument::fromJson(response.text.c_str(),&jsonError);
			 if (jsonError.error != QJsonParseError::NoError)
			 {
				 printf("Update Aria2 Error %s on %s\n", response.text.c_str(), task.urls[i].c_str());
				 continue;
			 }
			 auto root = doc.object();
			 if (root.contains("result"))
			 {
				 task.aria_id[i] = root.value("result").toString().toStdString();
				 update_success_ct++;
			 }
		}
		if (init)
			tmp_task_map.insert(task_pair);
	}
	if (init)
	{
		printf("Start Download %d/%d works\n",(int)tmp_task_map.size(), (int)task_map.size());
		task_map = tmp_task_map;
	}
	printf("%d(Running)/%d(Updating)/%d(Done)/%d(Unknown) in %d files. %d/%d updated\n",running_ct,update_ct,complete_ct,unknown_ct,total_ct,update_success_ct,update_ct);
	return complete_ct == total_ct;
}

QString DLSiteClient::unicodeToString(const QString& str)
{
	QString result=str;
	int index = result.indexOf("\\u");
	while (index != -1)
	{
		result.replace(result.mid(index, 6),QString(result.mid(index + 2, 4).toUShort(0, 16)).toUtf8());
		index = result.indexOf("\\u");
	}
	return result;
}

bool DLSiteClient::RenameFile(const QString& file,const QString& id,const QString&work_name)
{
	QString name = unicodeToString(work_name);
	name.replace(QRegExp("[/\\\\?*<>:\"|.]"), "_");
	int pos = file.lastIndexOf(QRegExp("[\\/]")) + 1;
	QString old_name = QDir(file).dirName();
	QString new_name = id + " " + name;
	if (old_name == new_name)
		return false;
	QDir parent(file);
	parent.cdUp();
	while (parent.exists(new_name))
		new_name = new_name + "_repeat";
	if (old_name == new_name)
		return false;
	if (parent.rename(old_name, new_name))
		return true;
	qDebug() << "Cant Rename " << old_name << "->" << new_name;
	return false;
}
void DLSiteClient::RenameThread(QStringList local_files)
{
	cpr::Session session;//����Ҫcookie
	session.SetVerifySsl(cpr::VerifySsl{ false });
	session.SetProxies(cpr::Proxies{ {std::string("https"), std::string("127.0.0.1:8000")} });
	session.SetHeader(cpr::Header{ {"user-agent","Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.88 Safari/537.36"} });
	session.SetRedirect(true);

	int ct=0;
	auto reg = DBProxyServer::GetWorkNameExp();
	for (const auto& file : local_files)
	{
		reg.indexIn(file);
		QString id = reg.cap(0);
		std::string url = format("https://www.dlsite.com/maniax/product/info/ajax?product_id=%s&cdn_cache_min=0", id.toStdString().c_str());
		session.SetUrl(url);
		auto res=session.Get();
		if (res.status_code == 200)
		{
			std::smatch result;
			if (std::regex_search(res.text, result, std::regex("\"work_name\"[:\" ]*([^\"]*)\",")))
			{
				std::string file_name = result[1];
				ct += RenameFile(file, id, QString::fromLocal8Bit(file_name.c_str()));
			}
		}
	}
	running = false;
}
void DLSiteClient::DownloadThread(QStringList works)
{
	task_map.clear();
	std::map<std::string, std::future<State>> futures;
	QStringList tmp;
//	works = QStringList{ "RJ317750" };
	for (const auto& id : works)
		futures[id.toStdString()] = std::async(&TryDownloadWork, id.toStdString(), main_cookies,user_agent,false);
	for (auto& pair : futures)//Ӧ����whenall,���ǲ�û��
		task_map[pair.first.c_str()] = pair.second.get();
	CheckTaskStatus(true);
	do
		Sleep(1000 * 60*30);
	while (!CheckTaskStatus(false));
	running = false;
}
DLSiteClient::State DLSiteClient::TryDownloadWork(std::string id,cpr::Cookies cookie, cpr::UserAgent user_agent, bool only_refresh_cookie) {
	DLSiteClient::State task;
	cpr::Session session;
	session.SetVerifySsl(cpr::VerifySsl{ false });
	session.SetProxies(cpr::Proxies{ {DLConfig::REQUEST_PROXY_TYPE, DLConfig::REQUEST_PROXY} });
	session.SetCookies(cookie);
	session.SetUserAgent(user_agent);
	//url�ĵ�һ������������Ϊmanix��pro��books������ʵ�ʿ��������
	//��Ʒҳ��ĵڶ��������Ƿ��۷�Ϊwork��announce,���صĿ϶�����work
	//��ȡ����
	{
		session.SetRedirect(true);
		std::string url=format("https://www.dlsite.com/maniax/work/=/product_id/%s.html",id.c_str());
		session.SetUrl(cpr::Url{ url });
		auto res=session.Get();
		if (res.status_code == 200)
			task.type = GetWorkTypeFromWeb(res.text,id);
		else
			task.type = WorkType::UNKNOWN;
	}
	if (task.type == WorkType::CANTDOWNLOAD)
		return task;
	//��ȡ��ʵ����
	session.SetRedirect(false);//����ʹ���Զ��ض�����Ϊ��Ҫ��¼�ض���ǰ��set-cookie�Լ��б��Ƿ�ֶ�����
	std::string url = DLSiteClient::format("https://www.dlsite.com/maniax/download/=/product_id/%s.html", id.c_str());
	int idx = 0;//��ǰ������0��ʾ����/��ε�������ҳ��1~n��ʾ������صĵ�1~nҳ
	while (true) {
		session.SetUrl(cpr::Url{ url });
		auto res = session.Head();
		if (res.header.count("set-cookie"))
		{
			task.cookie = "";
			cpr::Cookies download_cookie = cookie;
			QString new_cookie = res.header["set-cookie"].c_str();
			for (auto&pair : new_cookie.split(';'))
			{
				pair = pair.trimmed();
				if (pair.size() > 1)
				{
					auto tmp = pair.split('=');
					download_cookie[tmp[0].toStdString()] = tmp[1].toStdString();
				}
			}
			for (auto&pair : download_cookie)
				task.cookie += (pair.first + "=" + pair.second + "; ").c_str();
			if (only_refresh_cookie)
				return task;
		}
		if (res.status_code == 302)
		{
			if (res.header["location"] == "Array")//�ֶ����أ���ʱ��ʼ��ַ��һ��(Ϊhttps://www.dlsite.com/maniax/download/split/=/product_id/%1.html),��ͼ�õ������ص�url����ʱ���ض���"Array"
												  //û��Ҫ��ȡ������ҳ��ֱ�Ӵӵ�һ�ο�ʼ����
				url = DLSiteClient::format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html",++idx, id.c_str());
			else//һ���ض���
				url = res.header["location"];
		}
		else if (res.status_code == 200)
		{
			QString type = res.header["Content-Type"].c_str();
			if (type.contains("application/zip") || type.contains("binary/octet-stream") 
				|| type.contains("application/octet-stream") || type.contains("application/octet-stream") 
				|| type.contains("application/x-msdownload") || type.contains("application/x-rar-compressed"))//��ȡ����ʵ��ַ
			{
				std::string ext;
				std::string file_name;
				std::regex reg("filename=\"(.*)\"");
				auto header_disposition = res.header["Content-Disposition"];
				//���ļ�ͷ�г������ļ���
				std::smatch reg_result;
				if (std::regex_search(header_disposition, reg_result, reg))
					file_name = reg_result[1];//0�����������1�ǵ�һ������
				else//����ַ����
				{
					std::regex reg("file/(.*)/");
					if (std::regex_match(url, reg_result, reg))
						file_name = reg_result[1];
				}
				int pos = file_name.find_last_of('.');
				if (pos >= 0)
					ext = file_name.substr(pos+1);

				task.file_names.push_back(file_name);
				task.urls.push_back(url);
				if (!ext.empty())
					task.download_ext.insert(ext);

				if (idx == 0)//��������
				{
					task.ready = true;
					break;
				}
				else //������أ�������һ��
					url = DLSiteClient::format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html", ++idx, id.c_str());
			}
			else//ʧ�ܣ����type��html��ͨ������Ϊ��¼ʧ��/��������ҳʧ�ܣ�����ע��ҳ/����ҳ
				break;
		}
		else if (res.status_code == 404 && idx > 1)//������ص���ĩβ
		{
			task.ready = true;
			break;
		}
		else//ʧ��
			break;
	}
	if (!task.ready)
		qDebug() << "Receive " << id.c_str() << " Error:";
	task.aria_id.resize(task.urls.size(), "");
	return task;
}

DLSiteClient::WorkType DLSiteClient::GetWorkTypeFromWeb(const std::string& page,const std::string&id)
{
	/*
	<div class="work_genre">
	<a href="https://www.dlsite.com/maniax/fsr/=/work_category%5B0%5D/doujin/file_type/IJP/from/icon.work">
	<span class="icon_IJP" title="JPEG">JPEG</span></a>
	<a href="https://www.dlsite.com/maniax/fsr/=/work_category%5B0%5D/doujin/options/WPD/from/icon.work">
	<span class="icon_WPD" title="PDFͬ��">PDFͬ��</span>
	</a>
	</div>*/
	std::set<std::string> types;
	std::regex reg("class=\"icon_([A-Z]{1,10})\"");
	int cursor = 0;
	int start = -1;
	//�ҵ�����work_genre���div�������ͼ����
	while ((start = page.find("class=\"work_genre\">", cursor)) > 0)
	{
		start += std::string("class=\"work_genre\"").size();
		int end = page.find("</div>", start);
		std::string div = page.substr(start, end - start);
		std::smatch result;
		for(auto div_cursor = div.cbegin();std::regex_search(div_cursor,div.cend(),result,reg);
			div_cursor = result[0].second)
			types.insert(result[1]);
		cursor = end;
	}
	//���ȸ����ļ���ʽ�жϣ���θ�����Ʒ�����ж�
	//��Ϊ�е���Ʒ��ʽ��һ��Ĳ�һ��
	if (types.count("WBO"))//�ļ���ʽ:�����ר�ã������� ��������
		return WorkType::CANTDOWNLOAD;
	else if (types.count("EXE"))//�ļ���ʽ:���
		return WorkType::PROGRAM;
	else if (types.count("MP4"))//�ļ���ʽ:MP4
		return WorkType::VIDEO;
	else if (types.count("MP3") || types.count("WAV"))//�ļ���ʽ:MP3��WAV
		return WorkType::AUDIO;
	else if (types.count("IJP") || types.count("PNG") || types.count("IBP") || types.count("HTI"))//�ļ���ʽ:JPEG��BMP��HTML+ͼƬ
		return WorkType::PICTURE;
	else if (types.count("SOU") || types.count("MUS"))//��Ʒ����:����������(�������͵���Ʒ����������MS2)
		return WorkType::AUDIO;
	else if (types.count("RPG") || types.count("ACN") || types.count("STG") || types.count("SLN") || types.count("ADV")
		|| types.count("DNV") || types.count("PZL") || types.count("TYP") || types.count("QIZ")
		|| types.count("TBL") || types.count("ETC"))
		//��Ʒ����:RPG�������������ģ�⡢ð�ա�����С˵������(��tmR18����)�����֡����ա����桢����
		return WorkType::PROGRAM;
	else if (types.count("MOV"))//��Ʒ����:��Ƶ
		return WorkType::VIDEO;
	else if (types.count("MNG") || types.count("ICG"))//��Ʒ����:������CG
		return WorkType::PICTURE;
	else if (types.count("PVA"))//�ļ���ʽ:ר���������ͨ����WPDһ����ֲ�����ͼƬ���ͣ����ǲ�����ͬͼƬһ��򿪣���������
		return WorkType::OTHER;
	else if (types.count("WPD") || types.count("PDF") || types.count("icon_HTF"))//�ļ���ʽ:ר���������PDFͬ����PDF��HTML(flash)���޷�ȷ������
		return WorkType::OTHER;
	else if (types.count("AVI"))//�ļ���ʽ:AVI���޷�ȷ������
		return WorkType::OTHER;
	else if (types.count("ET3"))//��Ʒ����:����,�޷�ȷ������
		return WorkType::OTHER;
	printf("Cant identify type: %s",id.c_str());
	return WorkType::OTHER;
}

void DLSiteClient::StartDownload(const QByteArray& _cookies, const QByteArray& _user_agent, const QStringList& works)
{
	if (this->running)
		return;
	//��Ϊ���������߳����У��������ﲻ��Ҫ��ԭ�Ӳ���
	running = true;
	this->user_agent = cpr::UserAgent(_user_agent.toStdString());
	this->main_cookies = cpr::Cookies();
	for (auto&pair : _cookies.split(';'))
	{
		pair = pair.trimmed();
		if (pair.size() > 1)
		{
			auto tmp = pair.split('=');
			main_cookies[tmp[0].toStdString()] = tmp[1];
		}
	}
	//DownloadThread(pIDM, cookies, works);
	std::thread thread(&DLSiteClient::DownloadThread, this,works);
	thread.detach();
}

std::string DLSiteClient::format(const char* format, ...)
{
	char buff[1024*24] = { 0 };

	va_list args;
	va_start(args, format);
	vsprintf_s(buff, sizeof(buff), format, args);
	va_end(args);

	std::string str(buff);
	return str;
}
void DLSiteClient::StartRename(const QStringList& _files)
{
	if (this->running)
		return;
	//��Ϊ���������߳����У��������ﲻ��Ҫ��ԭ�Ӳ���
	running = true;
	QStringList local_files;
	auto reg = DBProxyServer::GetWorkNameExp();
	for (const auto& file : _files)
	{
		reg.indexIn(file);
		QString id = reg.cap(0);
		if(QDir(file).dirName().size()<id.size()+2)//ԭ���Ѿ������ֵĲ���Ҫ������
			local_files.push_back(file);
	}
	std::thread thread(&DLSiteClient::RenameThread,this,local_files);
	thread.detach();
}
