//module;
#include <QObject>
#include <QByteArray>
#include <QProcess>
#include <QStringList>
#include <set>
#include <vector>
#include "cpr/cpr.h"
#include <regex>
#include <future>
#include <ranges>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMetaType>
#include "cpr/cpr.h"
#include "Aria2Downloader.h"
#include "DLSiteClient.h"
//export module DLSiteClient;
import Util;
import DLConfig;
import DLSiteClientUtil;
class BaseDownloader;

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
	QString result = str;
	int index = result.indexOf("\\u");
	while (index != -1)
	{
		result.replace(result.mid(index, 6), QString(result.mid(index + 2, 4).toUShort(0, 16)).toUtf8());
		index = result.indexOf("\\u");
	}
	return result;
}

bool DLSiteClient::Extract(const QString& file_name, const QString& dir)
{
	auto process = new QProcess();
	process->setWorkingDirectory(QCoreApplication::applicationDirPath() + "/7z");
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
		{ "x",//解压,用e会失去目录结构
		file_name,
		"-o" + dir,//输出
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
	for (auto& task : task_list)//暂时只见到过单个zip和分段rar两种格式
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
		extract_dirs += dir;
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

bool DLSiteClient::RenameFile(const QString& file, const QString& id, const QString& work_name)
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
	LogError("Cant Rename %s -> %s", q2s(old_name).c_str(), q2s(new_name).c_str());
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
		QString id = GetIDFromDirName(file);
		auto work_info = GetWorkInfoFromDLSiteAPI(session, id).second;
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

QPair<QString, QJsonObject> DLSiteClient::GetWorkInfoFromDLSiteAPI(cpr::Session& session, const QString& id) {
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
			return { text,QJsonObject() };
		}
		return { text, doc.object().value(id).toObject() };
	}
	else {
		LogError("Cant Get work info:%s\n", q2s(id).c_str());
		return { "",QJsonObject() };
	}
}

void DLSiteClient::DownloadThread(QStringList works, cpr::Cookies cookie, cpr::UserAgent user_agent)
{
	std::vector<Task> task_list;
	std::map<std::string, std::future<Task>> futures;
	QStringList tmp;
	for (const auto& id : works)
		futures[q2s(id)] = std::async(&DLSiteClientUtil::MakeDownloadTask, q2s(id), cookie, user_agent);
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
	for (auto& pair : _cookies.split(';'))
	{
		pair = pair.trimmed();
		if (pair.size() > 1)
		{
			auto tmp = pair.split('=');
			cookies[q2s(tmp[0])] = q2s(tmp[1]);
		}
	}
	std::thread thread(&DLSiteClient::DownloadThread, this, works, cookies, cpr::UserAgent(_user_agent.toStdString()));
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
				if (item.isObject() && !item.toObject().empty())
				{
					QJsonObject object = item.toObject();
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
																	//[&]表示未显示声明捕获类型的变量都以引用捕获,[&session]表示显示声明session以引用捕获,此处session不应该拷贝
	for (const auto& work_info : parent_works | std::views::transform([&](auto& subwork_id) {return GetWorkInfoFromDLSiteAPI(session, subwork_id).second; }))
	{
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
			if (item.isString() && !ret.translations.contains(item.toString()))
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
		QString id = GetIDFromDirName(file);
		if (id.isEmpty())
			continue;
		if (QDir(file).dirName().size() < id.size() + 2)
			local_files.push_back(file);
	}
	std::thread thread(&DLSiteClient::RenameThread, this, local_files);
	thread.detach();
}