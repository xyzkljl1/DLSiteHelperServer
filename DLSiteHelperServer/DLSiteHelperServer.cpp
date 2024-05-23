#include "DLSiteHelperServer.h"
#include "mysql.h"
#include <qregularexpression.h>
#include <QDir>
#include <QUrl>
#include <ranges>
#include <string>
#include <map>
#include <set>
using namespace Util;
import DataBase;
const int SQL_LENGTH_LIMIT = 10000;
void IDontKnowWhy()
{
	//WHy?????
	//如果删掉这段代码，在GetAllInvalidWorksString末尾调用的类似代码就会无法编译,似乎是因为编译器无法确定唯一的|运算符
	//同样的代码在一个空文件中可以正常编译，在该文件考前位置可以正常编译
	//在该文件靠后的位置，如果之前没出现过同样代码，就会编译失败，如果之前出现过则可以编译
	std::set<std::string> invalid_work;
	auto x=invalid_work | std::views::join_with(' ');
}
DLSiteHelperServer::DLSiteHelperServer(QObject* parent):qserver(parent)
{
	SyncLocalFileToDB();//启动时立刻更新本地文件
	daily_timer.setInterval(86400*1000);
	daily_timer.start();
	//每天更新本地文件并获取workinfo
	connect(&daily_timer, &QTimer::timeout, this, &DLSiteHelperServer::DailyTask);
	qserver.route("/", [this](const QHttpServerRequest& request, QHttpServerResponder&& response) { this->HandleRequest(request,std::move(response)); });
	//通过HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Services/Tcpip/Parameters/ReservedPorts项将端口设为保留
	qserver.listen(QHostAddress::Any, DLConfig::SERVER_PORT);
}

DLSiteHelperServer::~DLSiteHelperServer()
{
}


void DLSiteHelperServer::HandleRequest(const QHttpServerRequest & request, QHttpServerResponder&& response)
{
	QString request_target ="/?"+request.query().toString();
	QRegExp reg_mark_eliminated(("(/\?markEliminated)(" +WORK_NAME_EXP+ ")").c_str());
	QRegExp reg_mark_special_eliminated(("(/\?markSpecialEliminated)("+WORK_NAME_EXP+")").c_str());
	QRegExp reg_mark_overlap(("(/\?markOverlap)&main=("+WORK_NAME_EXP+")&sub=("+WORK_NAME_EXP+")&duplex=([0-9])").c_str());
	if (request_target.startsWith("/?QueryInvalidDLSite"))
	{
		auto ret = GetAllInvalidWorksString();
		ReplyText(std::move(response), QHttpServerResponder::StatusCode::Ok, ret);
		Log("std::move(response) Query Request\n");
	}
	else if (request_target.startsWith("/?QueryOverlapDLSite"))
	{
		auto ret = GetAllOverlapWorksString();
		ReplyText(std::move(response), QHttpServerResponder::StatusCode::Ok, ret);
		Log("std::move(response) Query Request\n");
	}
	else if (request_target.startsWith("/?Download"))
	{
		Log("Trying to start download,this may take few minutes");
		SyncLocalFileToDB();
		//user-agent与cookie需要符合，user-agent通过请求的user-agent，cookie通过请求的data获得
		//request.headers()["user-agent"]
		QByteArray useragent;
		for (auto& pair : request.headers())
			if (pair.first.toLower() == "user-agent")
				useragent = pair.second;
		DownloadAll(request.body(), useragent);
		ReplyText(std::move(response), QHttpServerResponder::StatusCode::Ok, "Started");
		Log("std::move(response) Download Request\n");
	}
	else if (request_target.startsWith("/?RenameLocal")) {
		RenameLocal();
		ReplyText(std::move(response), QHttpServerResponder::StatusCode::Ok, "Started");
		Log("std::move(response) Rename Request\n");
	}
	else if (request_target.startsWith("/?UpdateBoughtItems"))
	{
		auto ret = UpdateBoughtItems(request.body());
		ReplyText(std::move(response), QHttpServerResponder::StatusCode::Ok, ret);
		Log("Update Bought Items\n");
	}
	else if (reg_mark_eliminated.indexIn(request_target) > 0)
	{
		if (!reg_mark_eliminated.cap(2).isEmpty())
		{
			std::string id = q2s(reg_mark_eliminated.cap(2));
			DataBase database;
			std::string  cmd = "INSERT IGNORE INTO works(id) VALUES(\"" + id + "\");"
				"UPDATE works SET eliminated = 1 WHERE id = \"" + id + "\"; ";
			mysql_query(&database.my_sql, cmd.c_str());
			if (mysql_errno(&database.my_sql))
			{
				LogError("%s\n", mysql_error(&database.my_sql));
				ReplyText(std::move(response), QHttpServerResponder::StatusCode::NotFound, "SQL Fail");
			}
			else
			{
				ReplyText(std::move(response), QHttpServerResponder::StatusCode::Ok, "Success");
				Log("Mark %s Eliminated\n", id.c_str());
			}
		}
	}
	else if (reg_mark_special_eliminated.indexIn(request_target) > 0)
	{
		if (!reg_mark_special_eliminated.cap(2).isEmpty())
		{
			std::string id = q2s(reg_mark_special_eliminated.cap(2));
			DataBase database;
			std::string  cmd = "INSERT IGNORE INTO works(id) VALUES(\"" + id + "\");"
				"UPDATE works SET specialEliminated = 1 WHERE id = \"" + id + "\"; ";
			mysql_query(&database.my_sql, cmd.c_str());
			if (mysql_errno(&database.my_sql))
			{
				LogError("%s\n", mysql_error(&database.my_sql));
				ReplyText(std::move(response), QHttpServerResponder::StatusCode::NotFound, "SQL Fail");
			}
			else
			{
				ReplyText(std::move(response), QHttpServerResponder::StatusCode::Ok, "Success");
				Log("Mark %s Special Eliminated\n", id.c_str());
			}
		}
	}
	else if (reg_mark_overlap.indexIn(request_target) > 0)
	{
		std::string main_id = q2s(reg_mark_overlap.cap(2));
		std::string sub_id = q2s(reg_mark_overlap.cap(3));
		bool duplex = reg_mark_overlap.cap(4).toInt();
		DataBase database;
		std::string cmd = "INSERT IGNORE INTO works(id) VALUES(\"" + main_id + "\");"
			"INSERT IGNORE INTO works(id) VALUES(\"" + sub_id + "\");"
			"INSERT IGNORE INTO overlap VALUES(\"" + main_id + "\",\"" + sub_id + "\");"
			+ (duplex ? std::string("INSERT IGNORE INTO overlap VALUES(\"" + sub_id + "\",\"" + main_id + "\");") : std::string());
		mysql_query(&database.my_sql, cmd.c_str());
		if (mysql_errno(&database.my_sql))
		{
			LogError("%s\n", mysql_error(&database.my_sql));
			ReplyText(std::move(response), QHttpServerResponder::StatusCode::NotFound, "SQL Fail");
		}
		else
		{
			ReplyText(std::move(response), QHttpServerResponder::StatusCode::Ok, "Success");
			Log("Mark Overlap %s%s %s\n", duplex ? "Duplex " : "", main_id.c_str(), sub_id.c_str());
		}
	}
}

void DLSiteHelperServer::ReplyText(QHttpServerResponder&& responder,const QHttpServerResponder::StatusCode&status, const QString & message)
{
	responder.write(message.toLocal8Bit(), QHttpServerResponder::HeaderList{ {"Content-Type", "text/plain"},{"Access-Control-Allow-Origin", "*"} }, status);
}

QString DLSiteHelperServer::GetAllOverlapWorksString()
{
	auto overlap_work= GetAllOverlapWorks();//std::map<std::string, std::set<std::string>>
	std::string ret="{";
	//用ranges之后似乎变得更复杂了
	ret+=overlap_work	| std::views::filter([](auto& pair) {return !pair.second.empty(); })
						| std::views::transform([](auto& pair) {
								return Format("\"%s\":[%s]",pair.first.c_str(),
											(pair.second | std::views::transform([](auto& str) {return  Format("\"%s\"",str.c_str()); })
												| std::views::join_with(',')
												| std::ranges::to<std::string>()).c_str()
											);
							})
						| std::views::join_with(',')
						| std::ranges::to<std::string>();
	/*
	for (auto& pair : overlap_work)
		if (!pair.second.empty())
		{
			ret += "\"" + pair.first + "\":[";
			for (auto& sub : pair.second)
				ret += "\"" + sub + "\",";
			ret.pop_back();//去掉最后一个逗号
			ret += "],";
		}
	if(ret.size()>2)//去掉最后一个逗号
		ret.pop_back();
	*/
	ret += "}";
	printf("%s", ret.c_str());
	return s2q(ret);
}

std::map<std::string, std::set<std::string>> DLSiteHelperServer::GetAllOverlapWorks()
{
	DataBase database;
	std::map<std::string, std::set<std::string>> overlap_work;
	//获得手动标记的覆盖关系
	std::map<std::string, std::set<std::string>> base_edges;
	for (const auto& pair : database.SelectOverlaps())
		base_edges[pair.first].insert(pair.second);
	//推导出其它覆盖关系
	overlap_work = base_edges;
	for (auto& pair : overlap_work)
	{
		auto& ret = pair.second;
		//对于每个work，基于base_edges宽搜
		std::set<std::string> starts = ret;
		std::set<std::string> next_starts;
		do {
			for (auto& s : starts)
				if (base_edges.count(s))
					for (auto& t : base_edges[s])
						if (t != pair.first && !ret.count(t))
						{
							ret.insert(t);
							next_starts.insert(t);
						}
			starts = next_starts;
			next_starts.clear();
		} while (!starts.empty());
	}
	return overlap_work;
}

void DLSiteHelperServer::DailyTask()
{
	FetchWorkInfo(500);
	SyncLocalFileToDB();
}

QString DLSiteHelperServer::UpdateBoughtItems(const QByteArray & data)
{
	DataBase database;
	QRegExp reg(WORK_NAME_EXP.c_str());
	std::string	cmd;
	for (const auto& byte : data.split(' '))
	{
		std::string id = q2s(byte);
		if (reg.exactMatch(QString(byte)))
			cmd += "INSERT IGNORE INTO works(id) VALUES(\"" + id + "\");"
			"UPDATE works SET bought = 1 WHERE id = \"" + id + "\"; ";
		if (cmd.length() > SQL_LENGTH_LIMIT)
			database.SendQuery(cmd);
	}
	if (cmd.length() > 0)			
		database.SendQuery(cmd);

	int ret = database.SelectWorksCount(true,-1, -1, 1);
	Log("Update Bought List %d\n", ret);
	return "Sucess";
}

QString DLSiteHelperServer::GetAllInvalidWorksString()
{
	std::set<std::string> invalid_work;
	DataBase database;
	//覆盖的作品全部invalid的作品未必是invalid，因为可能有额外的内容
	//被非invalid覆盖的作品不是invalid，因为可能有合并和分开购买的不同需求	
	{//已下载/标记/购买的作品是invalid
		std::vector<std::string> tmp=database.SelectWorksId(false, 1, 1, 1);
		invalid_work.insert(tmp.begin(), tmp.end());
	}
	{//被invalid作品覆盖的作品也是invalid
		auto overlaps = GetAllOverlapWorks();
		auto tmp = invalid_work;
		for (auto& i : tmp)
			if (overlaps.count(i))
				for (auto& j : overlaps[i])
					if (!invalid_work.count(j))
						invalid_work.insert(j);
	}
	{//最后加入specialEliminated，SpecialEliminate的作品不会令覆盖的作品变为invalid所以要放在后面加入
		std::vector<std::string> tmp = database.SelectWorksId(false, -1, -1, -1, 1);
		for (auto& id : tmp)
			if (!invalid_work.count(id))
				invalid_work.insert(id);
	}
	//std::string ret;
	//for(const auto& id:invalid_work)
	//	ret += (ret.empty() ? "" : " ") + id;
	//return s2q(ret);
	return s2q(invalid_work | std::views::join_with(' ') |std::ranges::to<std::string>());
}

void DLSiteHelperServer::SyncLocalFileToDB()
{
	std::string cmd;
	DataBase database;
	std::set<std::string> overlapped_works;//被已下载/已排除作品覆盖的作品
	std::set<std::string> downloaded_works;//已下载作品
	std::map<std::string, QString> currentdir_downloaded_works;//当前目录的已下载作品，id-dir

	auto overlaps=GetAllOverlapWorks();
	{//获取eliminated及其覆盖的作品
		std::vector<std::string> eliminated_works = database.SelectWorksId(false, 1);
		overlapped_works.insert(eliminated_works.begin(),eliminated_works.end());
		for (auto& i : eliminated_works)
			if (overlaps.count(i))
				for (auto& j : overlaps[i])
					if (!overlapped_works.count(j))
						overlapped_works.insert(j);
	}
	//依序遍历local_dirs,查找已下载的作品去重，如果下载了多个互相覆盖的作品，保留靠前的目录中的文件
	for (auto& root_dir : DLConfig::local_dirs)
	{
		currentdir_downloaded_works.clear();
		for (auto& dir : GetLocalFiles(QStringList{ root_dir }))
		{
			std::string id = q2s(GetIDFromDirName(QFileInfo(dir).fileName()));
			if (id.empty())
				continue;
			if (downloaded_works.count(id) || overlapped_works.count(id))
			{
				//删除不需要的
				//按目录优先级遍历，如果作品覆盖了同目录/低优先级目录的作品则删除被覆盖的作品
				//不能因低优先级目录的作品的删掉高优先级目录的作品，防止一个收藏作品因为新下载了一个合集就被从收藏文件夹里移除
				//同目录覆盖作品会因为遍历顺序漏掉一部分，例如A单向覆盖B，先遍历B再遍历A，则B不会在此处被删除，在下方补充判断currentdir_downloaded_works
				if (!QDir(dir).removeRecursively())
					Log("Can't Remove %s\n", q2s(dir).c_str());
				else
					Log("Remove Eliminated: %s\n", q2s(dir).c_str());
			}
			else
			{
				downloaded_works.insert(id);
				currentdir_downloaded_works[id] = dir;
				//检查该作品覆盖的作品
				if (overlaps.count(id))
					for (auto& sub_id : overlaps[id])
					{
						if (sub_id == id)
							continue;
						//加入overlapped_works
						if (!overlapped_works.count(sub_id))
							overlapped_works.insert(sub_id);
						//如果覆盖了同目录下其它作品，则把之前的作品删除(该作品进入了该if说明两者不是双向覆盖，而是该作品单向覆盖之前的作品)
						if (currentdir_downloaded_works.count(sub_id))
						{
							if (!QDir(currentdir_downloaded_works[sub_id]).removeRecursively())
								Log("Can't Remove %s\n", q2s(currentdir_downloaded_works[sub_id]).c_str());
							else
								Log("Remove Eliminated: %s\n", q2s(currentdir_downloaded_works[sub_id]).c_str());
							currentdir_downloaded_works.erase(sub_id);
							downloaded_works.erase(sub_id);
						}
					}
			}
		}

	}
	//downloaded写入数据库
	{
		//重置所有作品downloaded状态
		cmd = "UPDATE works set downloaded=0;";
		for (const auto& id : downloaded_works)
		{
			cmd += "INSERT IGNORE INTO works(id) VALUES(\"" + id + "\");"
				"UPDATE works SET downloaded = 1 WHERE id = \"" + id + "\"; ";
			if (cmd.length() > SQL_LENGTH_LIMIT)
				database.SendQuery(cmd);
		}
		if (cmd.length() > 0)
			database.SendQuery(cmd);
	}
	Log("Sync from local %zd works\n", downloaded_works.size());
}

void DLSiteHelperServer::FetchWorkInfo(int limit)
{
	//获取尚未获取过info的作品的info，据此获取其翻译信息并标记为覆盖，然后将info存入数据库
	//目前存info仅是为了备用以及区分哪些作品处理过
	//info不需要更新，如果新的多语言版本出现，获取新作品的info时就可以得知翻译信息
	//info为null表示没有获取过，为空表示获取失败
	//有的子作品（如RJ01000889）无法获取到info，但是对父作品/其它语言版本获取info的时候也能获取覆盖关系，所以不影响
	DataBase database;
	std::string cmd;
	int ct1 = 0;
	int ct2 = 0;
	auto id_list=database.SelectWorksIdWithoutInfo(limit);
	std::map<std::string, std::string> id_info_map;
	for (const auto& id : id_list)
	{
		DLSiteClient::WorkInfo res=client.FetchWorksInfo(s2q(id));
 		QStringList translations = res.translations;
		//如果有多语言版本则标记覆盖
		if (translations.count() > 1)
		{
			ct1++;
			ct2 +=translations.count();
			//两两标记为双向覆盖
			for (int i = 0; i < translations.count(); ++i)
				for (int j = i + 1; j < translations.count(); ++j)
					cmd += database.GetSQLMarkOverlap(q2s(translations[i]), q2s(translations[j]), true);
		}
		//如果获取到了info则储存
		if (!res.work_info_text.isEmpty())
			id_info_map[id] = q2s(res.work_info_text);
		else//没获取到就存空字符串，因为该信息并不重要，不要了也没什么损失，存成空防止反复fetch
			id_info_map[id] = "";
		//如果是乙女向则直接排除
		if (res.is_otm)
			cmd += database.GetSQLUpdateWork(id, 1);

		if (cmd.length() > SQL_LENGTH_LIMIT)
			database.SendQuery(cmd);
	}
	if (cmd.length() > 0)
		database.SendQuery(cmd);
	database.SetWorksInfo(id_info_map);
	Log("Fetched %zd. Marked %d group/%d works of translation.", id_info_map.size(), ct1, ct2);
}

void DLSiteHelperServer::DownloadAll(const QByteArray&cookie, const QByteArray& user_agent)
{
	QStringList works;
	DataBase database;
	std::set<std::string> not_expected_works;//不需要下载的
	{//eliminated/downloaded及其覆盖的作品不需要下载
		auto overlaps = GetAllOverlapWorks();
		std::vector<std::string> eliminated_works = database.SelectWorksId(false, 1,1);

		not_expected_works.insert(eliminated_works.begin(),eliminated_works.end());
		for (auto& i : eliminated_works)
			if (overlaps.count(i))
				for (auto& j : overlaps[i])
					if (!not_expected_works.count(j))
						not_expected_works.insert(j);
	}
	{//查找已购买且需要下载的作品
		//eliminated=0 & downloaded=0 & bought=1
		std::vector<std::string> bought_works = database.SelectWorksId(true, 0,0,1);
		for(const auto& id:bought_works)
			if (!not_expected_works.count(id))
				works.push_back(s2q(id));
	}
	client.StartDownload(cookie,user_agent,works);
}

void DLSiteHelperServer::RenameLocal()
{
	client.StartRename(GetLocalFiles(DLConfig::local_dirs + DLConfig::local_tmp_dirs));
}

//获得指定根目录下所有work的路径
//遍历根目录下一级以及根目录下[以SERIES_NAME_EXP开头的目录]的下一级
QStringList DLSiteHelperServer::GetLocalFiles(const QStringList& root)
{
	QStringList ret;
	QRegExp regex_is_work(WORK_NAME_EXP.c_str());
	QRegExp regex_is_series(SERIES_NAME_EXP.c_str());
	for (auto& dir : root)
		for (auto& info : QDir(dir).entryInfoList({ "*" }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
			if (regex_is_work.indexIn(info.fileName()) > -1)
				ret.push_back(info.filePath());
			else if (regex_is_series.indexIn(info.fileName()) > -1)
			{
				for (auto& sub_info : QDir(info.filePath()).entryInfoList({ "*" }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
					if (regex_is_work.indexIn(sub_info.fileName()) > -1)
						ret.push_back(sub_info.filePath());
			}
	return ret;
}
