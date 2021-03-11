#include "DLSiteClient.h"
#include "DBProxyServer.h"
#include <regex>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include "cpr/cpr.h"
Q_DECLARE_METATYPE(DLSiteClient::StateMap);//沙雕宏认不出来逗号，所以必须起个别名
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
		//TODO:关闭之前的aria2
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
bool DLSiteClient::CheckTaskStatus(bool init)
{
	if (init)//清理目标目录，防止错误的文件影响下载
		QDir(DLConfig::DOWNLOAD_DIR).removeRecursively();		
	/*
	aria2中出错的任务无法继续，只能创建一个新的替代它(会自动断点续传)
	如果任务太多导致旧的任务被释放，可能会令查询得不到正确的结果
	考虑到不会下载很多文件，在配置文件中将max-download-result调大就能避免这个问题
	*/
	cpr::Session session;
	session.SetUrl(cpr::Url{ QString("http://127.0.0.1:%1/jsonrpc").arg(DLConfig::ARIA2_PORT).toStdString()});

	StateMap tmp_task_map;
	//aria2任务计数
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
		//找到需要更新的子任务
		total_ct += task.urls.size();
		std::vector<int> update_index;//需要更新的子任务的索引
		for (int i = 0; i < task.aria_id.size(); ++i)//aria_id和url的size应该是一致的
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
		if (!init)//如果不是第一次创建任务，失效的任务可能是由于cookie过期，需要重新获取
			task.cookie=TryDownloadWork(task_pair.first, main_cookies, cpr::UserAgent{ user_agent.c_str() }, true).cookie;
		//更新子任务
		std::string path = DLConfig::DOWNLOAD_DIR.toLocal8Bit().toStdString();
		switch (task.type) {
		case WorkType::AUDIO:path += "ASMR/"; break;
		case WorkType::VIDEO:path += "Video/"; break;
		case WorkType::PICTURE:path += "CG/"; break;
		case WorkType::PROGRAM:path += "Game/"; break;
		default:path += "Default/";
		}
		//zip的解压完还需要转码，所以要和rar区分开
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
	cpr::Session session;//不需要cookie
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
	for (auto& pair : futures)//应该用whenall,但是并没有
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
	//url的第一级根据卖场分为manix、pro、books，但是实际可以随便用
	//产品页面的第二级根据是否发售分为work、announce,下载的肯定都是work
	//获取类型
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
	//获取真实连接
	session.SetRedirect(false);//不能使用自动重定向，因为需要记录重定向前的set-cookie以及判别是否分段下载
	std::string url = DLSiteClient::format("https://www.dlsite.com/maniax/download/=/product_id/%s.html", id.c_str());
	int idx = 0;//当前段数，0表示单段/多段的总下载页，1~n表示多段下载的第1~n页
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
			if (res.header["location"] == "Array")//分段下载，此时初始网址不一样(为https://www.dlsite.com/maniax/download/split/=/product_id/%1.html),试图用单段下载的url访问时会重定向到"Array"
												  //没必要获取总下载页，直接从第一段开始下载
				url = DLSiteClient::format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html",++idx, id.c_str());
			else//一般重定向
				url = res.header["location"];
		}
		else if (res.status_code == 200)
		{
			QString type = res.header["Content-Type"].c_str();
			if (type.contains("application/zip") || type.contains("binary/octet-stream") 
				|| type.contains("application/octet-stream") || type.contains("application/octet-stream") 
				|| type.contains("application/x-msdownload") || type.contains("application/x-rar-compressed"))//获取到真实地址
			{
				std::string ext;
				std::string file_name;
				std::regex reg("filename=\"(.*)\"");
				auto header_disposition = res.header["Content-Disposition"];
				//从文件头中尝试找文件名
				std::smatch reg_result;
				if (std::regex_search(header_disposition, reg_result, reg))
					file_name = reg_result[1];//0是完整结果，1是第一个括号
				else//从网址中找
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

				if (idx == 0)//单段下载
				{
					task.ready = true;
					break;
				}
				else //多段下载，尝试下一段
					url = DLSiteClient::format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html", ++idx, id.c_str());
			}
			else//失败，如果type是html，通常是因为登录失败/访问下载页失败，返回注册页/错误页
				break;
		}
		else if (res.status_code == 404 && idx > 1)//多段下载到达末尾
		{
			task.ready = true;
			break;
		}
		else//失败
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
	<span class="icon_WPD" title="PDF同捆">PDF同捆</span>
	</a>
	</div>*/
	std::set<std::string> types;
	std::regex reg("class=\"icon_([A-Z]{1,10})\"");
	int cursor = 0;
	int start = -1;
	//找到所有work_genre类的div里的所有图标类
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
	//优先根据文件形式判断，其次根据作品类型判断
	//因为有的作品格式跟一般的不一致
	if (types.count("WBO"))//文件形式:浏览器专用，不下载 ※最优先
		return WorkType::CANTDOWNLOAD;
	else if (types.count("EXE"))//文件形式:软件
		return WorkType::PROGRAM;
	else if (types.count("MP4"))//文件形式:MP4
		return WorkType::VIDEO;
	else if (types.count("MP3") || types.count("WAV"))//文件形式:MP3、WAV
		return WorkType::AUDIO;
	else if (types.count("IJP") || types.count("PNG") || types.count("IBP") || types.count("HTI"))//文件形式:JPEG、BMP、HTML+图片
		return WorkType::PICTURE;
	else if (types.count("SOU") || types.count("MUS"))//作品类型:音声、音乐(其它类型的作品含有音乐是MS2)
		return WorkType::AUDIO;
	else if (types.count("RPG") || types.count("ACN") || types.count("STG") || types.count("SLN") || types.count("ADV")
		|| types.count("DNV") || types.count("PZL") || types.count("TYP") || types.count("QIZ")
		|| types.count("TBL") || types.count("ETC"))
		//作品类型:RPG、动作、射击、模拟、冒险、电子小说、益智(神tmR18益智)、打字、解谜、桌面、其它
		return WorkType::PROGRAM;
	else if (types.count("MOV"))//作品类型:视频
		return WorkType::VIDEO;
	else if (types.count("MNG") || types.count("ICG"))//作品类型:漫画、CG
		return WorkType::PICTURE;
	else if (types.count("PVA"))//文件形式:专用浏览器，通常和WPD一起出现并且是图片类型，但是不能如同图片一般打开，视作其它
		return WorkType::OTHER;
	else if (types.count("WPD") || types.count("PDF") || types.count("icon_HTF"))//文件形式:专用浏览器、PDF同捆、PDF、HTML(flash)，无法确定类型
		return WorkType::OTHER;
	else if (types.count("AVI"))//文件形式:AVI，无法确定类型
		return WorkType::OTHER;
	else if (types.count("ET3"))//作品类型:其它,无法确定类型
		return WorkType::OTHER;
	printf("Cant identify type: %s",id.c_str());
	return WorkType::OTHER;
}

void DLSiteClient::StartDownload(const QByteArray& _cookies, const QByteArray& _user_agent, const QStringList& works)
{
	if (this->running)
		return;
	//因为都是在主线程运行，所以这里不需要用原子操作
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
	//因为都是在主线程运行，所以这里不需要用原子操作
	running = true;
	QStringList local_files;
	auto reg = DBProxyServer::GetWorkNameExp();
	for (const auto& file : _files)
	{
		reg.indexIn(file);
		QString id = reg.cap(0);
		if(QDir(file).dirName().size()<id.size()+2)//原来已经有名字的不需要重命名
			local_files.push_back(file);
	}
	std::thread thread(&DLSiteClient::RenameThread,this,local_files);
	thread.detach();
}
