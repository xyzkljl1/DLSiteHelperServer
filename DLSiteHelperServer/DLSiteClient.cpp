#include "DLSiteClient.h"
#include <regex>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaType>
#include "cpr/cpr.h"
#include "DLSiteHelperServer.h"
#include "Aria2Downloader.h"
Q_DECLARE_METATYPE(std::vector<Task>)
DLSiteClient::DLSiteClient()
{
	qRegisterMetaType<std::vector<Task>>();
	if (DLConfig::ARIA2_Mode)//本来是想做两个Downloader的，但是后来发现IDM不太行
		downloader = new Aria2Downloader();
	else
		downloader = new Aria2Downloader();
	connect(downloader, &BaseDownloader::signalDownloadAborted, this, &DLSiteClient::OnDownloadAborted);
	connect(downloader, &BaseDownloader::signalDownloadDone, this, &DLSiteClient::OnDownloadDone);
}

DLSiteClient::~DLSiteClient()
{
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

bool DLSiteClient::Extract(const QString & file_name, const QString & dir)
{
	auto process = new QProcess();
	process->setWorkingDirectory(QCoreApplication::applicationDirPath()+"/7z");
	process->setProcessChannelMode(QProcess::ProcessChannelMode::MergedChannels);
	process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
#ifdef _DEBUG
	//开启时无法接收到输出
	/*process->setCreateProcessArgumentsModifier(
		[](QProcess::CreateProcessArguments * args){
		args->flags |= CREATE_NEW_CONSOLE;
		args->startupInfo->dwFlags &= ~STARTF_USESTDHANDLES;
	});*/
#endif
	//通通不要加双引号，空格和括号无需处理
	process->start(
		QCoreApplication::applicationDirPath() + "/7z/7z.exe",
		{"x",//解压,用e会失去目录结构
		file_name,
		"-o"+dir,//输出
		"-aoa",//覆盖
		"-mcp=932",//SHIFT-JIS编码，只有日文zip必须指定编码，但指定了也不会令其它文件解压错误，所以统统加上该参数
		"-sccWIN"//输出字符集
		}, QIODevice::ReadOnly);
	process->waitForStarted();
	process->waitForFinished();
	QByteArray response;
	while (true)
	{
		process->waitForReadyRead();
		QByteArray tmp = process->readAll();
		if (tmp.isEmpty())
			break;
		response.append(tmp);
	}
	if (QString(response).indexOf("Everything is Ok") < 0)
		return false;
	return true;
}
void DLSiteClient::OnDownloadDone(std::vector<Task> task_list)
{
	if (!running)
		return;
	Log("Download Done,Begin extract\n");
	QStringList extract_dirs;
	for (auto&task : task_list)//暂时只见到过单个zip和分段rar两种格式
	{
		if (task.download_ext.count("zip"))
		{
			if (task.download_ext.size() != 1 || task.urls.size() != 1)
			{
				LogError("Extract Error On %s\n", task.id.c_str());
				continue;
			}
		}
		else if (task.download_ext.count("rar"))
		{
			if (task.download_ext.size() != 2 || task.urls.size() <= 1 || !task.download_ext.count("exe"))
			{
				LogError("Extract Error On %s\n", task.id.c_str());
				continue;
			}
		}
		else
		{
			LogError("Unknown File Format On %s\n", task.id.c_str());
			continue;
		}
		QString dir = s2q(task.GetExtractPath());
		if (!Extract(s2q(task.GetDownloadPath(0)), dir))
		{
			LogError("Extract Error On %s _0\n", task.id.c_str());
			QDir(dir).removeRecursively();
			continue;
		}
		for (int i = 0; i < task.urls.size(); ++i)
			QFile(s2q(task.GetDownloadPath(i))).remove();
		extract_dirs+=dir;
	}
	Log("%d/%d works Extracted\n", (int)extract_dirs.size(), (int)task_list.size());
	running = false;
	{
		running = true;//RenameThread结束时将running置空
		std::thread thread(&DLSiteClient::RenameThread, this, extract_dirs);
		thread.detach();
	}
}

void DLSiteClient::OnDownloadAborted()
{
	if (!running)
		return;
	LogError("Download Aborted\n");
	running = false;
}

bool DLSiteClient::RenameFile(const QString& file,const QString& id,const QString&work_name)
{
	QString name = unicodeToString(work_name);
	name.replace(QRegularExpression("[/\\\\?*<>:\"|.]"), "_");
	int pos = file.lastIndexOf(QRegularExpression("[\\/]")) + 1;
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
	LogError("Cant Rename %s -> %s",q2s(old_name).c_str(), q2s(new_name).c_str());
	return false;
}
void DLSiteClient::RenameThread(QStringList local_files)
{
	cpr::Session session;//不需要cookie
	session.SetVerifySsl(cpr::VerifySsl{ false });
	session.SetProxies(cpr::Proxies{ {std::string("https"), DLConfig::REQUEST_PROXY} });
	session.SetHeader(cpr::Header{ {"user-agent","Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.88 Safari/537.36"} });
	session.SetRedirect(true);

	int ct = 0;
	for (const auto& file : local_files)
	{
		QString id = DLSiteHelperServer::GetIDFromDirName(file);
		auto work_info=GetWorkInfoFromDLSiteAPI(session, id).second;
		if (work_info.isEmpty())
			continue;
		if (!work_info.contains("work_name"))
		{
			LogError("Cant Find Name\n");
			continue;
		}
		ct += RenameFile(file, id, work_info.value("work_name").toString());
	}
	running = false;
	Log("Rename Done\n");
}

QPair<QString,QJsonObject> DLSiteClient::GetWorkInfoFromDLSiteAPI(cpr::Session& session, const QString& id) {
	std::string url = Format("https://www.dlsite.com/maniax/product/info/ajax?product_id=%s&cdn_cache_min=0", q2s(id).c_str());
	session.SetUrl(url);
	auto response = session.Get();
	if (response.status_code == 200)
	{
		QJsonParseError error;
		QString text = s2q(response.text);
		QJsonDocument doc = QJsonDocument::fromJson(response.text.c_str(), &error);
		if (error.error != QJsonParseError::NoError || !doc.object().contains(id))
		{
			LogError("Cant Parse Json %s\n", q2s(id).c_str());
			return {text,QJsonObject() };
		}
		return { text, doc.object().value(id).toObject() };
	}
	else {
		LogError("Cant Get work info:%s\n", q2s(id).c_str());
		return {"",QJsonObject()};
	}
}

void DLSiteClient::DownloadThread(QStringList works,cpr::Cookies cookie,cpr::UserAgent user_agent)
{
	std::vector<Task> task_list;
	std::map<std::string, std::future<Task>> futures;
	QStringList tmp;
	for (const auto& id : works)
		futures[q2s(id)] = std::async(&TryDownloadWork, q2s(id), cookie,user_agent);
	for (auto& pair : futures)//应该用whenall,但是并没有
		task_list.push_back(pair.second.get());
	//有时会下载失败403，疑似是因为cookie失效，在chrome里手动尝试下载任意文件可解
	//TODO: fix it
	if (!downloader->StartDownload(task_list, cookie, user_agent))
	{
		LogError("Cant Start Download\n");
		running = false;
		return;
	}
	else
		Log("Fetch Done,Download Begin\n");
}
Task DLSiteClient::TryDownloadWork(std::string id,cpr::Cookies cookie, cpr::UserAgent user_agent) {
	Task task;
	task.id = id;
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
		std::string url=Format("https://www.dlsite.com/maniax/work/=/product_id/%s.html",id.c_str());
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
	//2023-12：maniax/download/=/product_id/%s.html不再能对多文件重定向，多文件需要直接访问maniax/download/split/=/product_id/%s.html
	//2023-12：分段下载时变为每个文件必须单独使用一个cookie,cookie决定下载哪个文件，例如使用part1的url+part2的cookie时下载的是第二个文件
	std::string url = Format("https://www.dlsite.com/maniax/download/=/product_id/%s.html", id.c_str());
	std::string current_cookie;
	int idx = 0;//当前段数，0表示单段/多段的总下载页，1~n表示多段下载的第1~n页
	while (true) {
		session.SetUrl(cpr::Url{ url });
		auto res = session.Head();
		if (res.header.count("set-cookie"))
		{
			current_cookie = "";
			cpr::Cookies download_cookie = cookie;
			QString new_cookie = res.header["set-cookie"].c_str();
			for (auto&pair : new_cookie.split(';'))
			{
				pair = pair.trimmed();
				if (pair.size() > 1)
				{
					auto tmp = pair.split('=');
					download_cookie[q2s(tmp[0])] = q2s(tmp[1]);
				}
			}
			for (auto&pair : download_cookie)
				current_cookie += (pair.first + "=" + pair.second + "; ").c_str();
		}
		if (res.status_code == 302)
		{
			//if (res.header["location"] == "Array")//分段下载，此时初始网址不一样(为https://www.dlsite.com/maniax/download/split/=/product_id/%1.html),试图用单段下载的url访问时会重定向到"Array"
												  //没必要获取总下载页，直接从第一段开始下载
			//	url = Format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html",++idx, id.c_str());
			//else
			url = res.header["location"];//一般重定向
		}
		else if (res.status_code == 500) {
			//如果正在访问初始下载页面且获得500，说明是分段下载，此时初始网址不一样(为https://www.dlsite.com/maniax/download/split/=/product_id/%1.html)
			//没必要获取总下载页，直接从第一段开始下载
			if (url.find("https://www.dlsite.com/maniax/download/=/product_id/") != std::string::npos)
				url = Format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html", ++idx, id.c_str());
			else
				break;
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
				task.cookies.push_back(current_cookie);
				if (!ext.empty())
					task.download_ext.insert(ext);

				if (idx == 0)//单段下载
				{
					task.ready = true;
					break;
				}
				else //多段下载，尝试下一段
					url = Format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html", ++idx, id.c_str());
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
	//有的作品会被删除，例:RJ320458
	if (!task.ready)
		qDebug() << "Receive " << id.c_str() << " Error:";
	return task;
}

WorkType DLSiteClient::GetWorkTypeFromWeb(const std::string& page,const std::string&id)
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
	std::regex reg("class=\"icon_([A-Z0-9]{1,10})\"");
	int cursor = 0;
	int start = -1;
	//找到所有work_genre类的div里的所有图标类,即"作品形式"、"ファイル形式"、"年齢指定"、"その他"
	//有的作品仅有作品形式如VJ015443
	while ((start = page.find("class=\"work_genre\"", cursor)) > 0)
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
	//注意顺序，根据その他、文件形式、作品形式判断
	//因为有的作品格式跟一般的不一致
	if (types.count("OTM"))//その他:乙女向け ※最优先
		return WorkType::SHIT;
	else if (types.count("WBO"))//文件形式:浏览器专用，不下载
		return WorkType::CANTDOWNLOAD;
	else if (types.count("EXE"))//文件形式:软件
		return WorkType::PROGRAM;
	else if (types.count("MP4")|| types.count("MWM"))//文件形式:MP4、WMV
		return WorkType::VIDEO;
	else if (types.count("ADO"))//文件形式:オーディオ(audio)
		return WorkType::AUDIO;
	else if (types.count("MP3") || types.count("WAV") || types.count("FLC") || types.count("WMA")|| types.count("AAC"))//文件形式:MP3、WAV、FLAC、WMA、AAC
		return WorkType::AUDIO;
	else if (types.count("IJP") || types.count("PNG") || types.count("IBP") || types.count("HTI"))//文件形式:JPEG、BMP、HTML+图片
		return WorkType::PICTURE;
	else if (types.count("SOU") || types.count("MUS"))//作品类型:音声sound、音乐music(其它类型的作品含有音乐是MS2)
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
	else if (types.count("WPD") || types.count("PDF") || types.count("HTF"))//文件形式:专用浏览器、PDF同捆、PDF、HTML(flash)，无法确定类型
		return WorkType::OTHER;
	else if (types.count("AVI"))//文件形式:AVI，无法确定类型
		return WorkType::OTHER;
	else if (types.count("ET3"))//作品类型:其它,无法确定类型
		return WorkType::OTHER;
	LogError("Cant identify type: %s",id.c_str());
	return WorkType::OTHER;
}

void DLSiteClient::StartDownload(const QByteArray& _cookies, const QByteArray& _user_agent, const QStringList& works)
{
	if (this->running)
	{
		LogError("Busy. Reject Download Request\n");
		return;
	}
	//因为都是在主线程运行，所以这里不需要用原子操作
	running = true;
	cpr::Cookies cookies;
	for (auto&pair : _cookies.split(';'))
	{
		pair = pair.trimmed();
		if (pair.size() > 1)
		{
			auto tmp = pair.split('=');
			cookies[q2s(tmp[0])] = q2s(tmp[1]);
		}
	}
	std::thread thread(&DLSiteClient::DownloadThread, this,works,cookies, cpr::UserAgent(_user_agent.toStdString()));
	thread.detach();
}

DLSiteClient::WorkInfo DLSiteClient::FetchWorksInfo(const QString& work_id)
{
	WorkInfo ret;
	cpr::Session session;//不需要cookie
	session.SetVerifySsl(cpr::VerifySsl{ false });
	session.SetProxies(cpr::Proxies{ {std::string("https"), DLConfig::REQUEST_PROXY} });
	session.SetHeader(cpr::Header{ {"user-agent","Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.88 Safari/537.36"} });
	session.SetRedirect(true);
	/*
	* 翻译作品分两种：独立，组合
	* 其中"组合"的translation_info的is_parent/is_child为真，parent_workno/child_worknos不为空，形如
	(RJ01018122子)"translation_info": {
	  "is_translation_agree": false,
	  "is_volunteer": false,
	  "is_original": false,
	  "is_parent": false,
	  "is_child": true,
	  "is_translation_bonus_child": false,
	  "original_workno": "RJ380627",
	  "parent_workno": "RJ01018121",
	  "child_worknos": [],
	  "lang": "CHI_HANT",
	  "production_trade_price_rate": 0,
	  "translation_bonus_langs": []
	}
	(RJ01018121父)"translation_info": {
	  "is_translation_agree": false,
	  "is_volunteer": false,
	  "is_original": false,
	  "is_parent": true,
	  "is_child": false,
	  "is_translation_bonus_child": false,
	  "original_workno": "RJ380627",
	  "parent_workno": null,
	  "child_worknos": [ "RJ01018122" ],
	  "lang": "CHI_HANT",
	  "production_trade_price_rate": 0,
	  "translation_bonus_langs": []
	},
	有的
	目前只见过一层组合关系	
	组合作品的父、子有各自的RJ号，使用父/子RJ号均能访问到该翻译作品的网页，但是子作品的url会被重定向至形如maniax/work/=/product_id/{父RJ号}.html/?translation={子RJ号}
	由于父作品也有lang，推测一个父作品的所有子作品都是同一语言
	一父可以有多子，如RJ01000889，但是没看出来多个子作品有什么区别
	部分子作品如RJ01000890无法获取到info，只有父作品可以获取到
	
	暂定：视作所有父子两两等价(即双向覆盖)
	*
	* 多语言版本在dl_count_items中形如
	"dl_count_items": [
	  {
		"workno": "RJ380627",
		"edition_id": 9798,
		"edition_type": "language",
		"display_order": 1,
		"label": "\u65e5\u672c\u8a9e",
		"dl_count": "2108",
		"display_label": "\u65e5\u672c\u8a9e"
	  },
	  {
		"workno": "RJ01018121",
		"edition_id": 9798,
		"edition_type": "language",
		"display_order": 7,
		"label": "\u7e41\u4f53\u4e2d\u6587",
		"dl_count": 85,
		"display_label": "\u7e41\u4f53\u4e2d\u6587"
	  }
	]
	其中总是仅包含独立作品和父作品，因此需要另外查询子作品
	包含自己
	edition_type似乎总是为language
	本体/追加voice/完全版/无料版等信息似乎不会出现在此处
	因此dl_count_items包含的作品及其子作品均视作两两等价
	*/
	QStringList parent_works;
	{
		auto work_info_pair = GetWorkInfoFromDLSiteAPI(session, work_id);
		ret.work_info_text = work_info_pair.first;
		QJsonObject work_info = work_info_pair.second;
		//if (work_info.isEmpty())//如果work_info.isEmpty()那么!work_info.contains("dl_count_items")一定为真
			//return ret;
		//检查标签
		if (work_info.contains("options"))
			if (work_info.value("options").toString().contains("OTM"))//乙女向
				ret.is_otm = true;
		//找到所有dl_count_items
		if (work_info.contains("dl_count_items"))
			for (const QJsonValue& item : work_info.value("dl_count_items").toArray())
				if(item.isObject()&&!item.toObject().empty())
				{
					QJsonObject object=item.toObject();
					QString type = object.value("edition_type").toString();
					if (type != "language")
					{
						throw "Unknown Situation";
					}
					parent_works += object.value("workno").toString();
				}
	}
	//找到所有dl_count_items包含的子作品
	ret.translations = parent_works;
	for (const auto& subwork_id : parent_works)
	{
		auto work_info = GetWorkInfoFromDLSiteAPI(session, subwork_id).second;
		//if (work_info.isEmpty())
			//return ret;
		if (!work_info.contains("translation_info"))
			return ret;
		auto translation_info = work_info.value("translation_info").toObject();
		if (translation_info.empty())
			continue;
		if (!translation_info.value("is_parent").toBool())//独立作品
			continue;
		if (translation_info.value("is_child").toBool())//不应该出现子作品
		{
			throw "Unknown Situation";
			return ret;
		}
		for (const QJsonValue& item : translation_info.value("child_worknos").toArray())
			if(item.isString()&&!ret.translations.contains(item.toString()))
				ret.translations += item.toString();
	}
	return ret;
}

void DLSiteClient::StartRename(const QStringList& _files)
{
	if (this->running)
		return;
	//因为都是在主线程运行，所以这里不需要用原子操作
	running = true;
	QStringList local_files;
	for (const auto& file : _files)
	{
		QString id = DLSiteHelperServer::GetIDFromDirName(file);
		if (id.isEmpty())
			continue;
		if (QDir(file).dirName().size() < id.size() + 2)
			local_files.push_back(file);
	}
	std::thread thread(&DLSiteClient::RenameThread,this,local_files);
	thread.detach();
}