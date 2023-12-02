#include "DLSiteHelperServer.h"
#include "DLSiteClient.h"
#include <QRegExp>
#include <QDir>
#include <QUrl>
#include <string>
#include <map>
#include <set>
#include "DataBase.h"

const int SQL_LENGTH_LIMIT = 10000;
#define WORK_NAME_EXP "[RVBJ]{2}[0-9]{3,8}"
//把字母和数字分别capture的表达式
#define WORK_NAME_EXP_SEP "([RVBJ]{2})([0-9]{3,8})"
#define SERIES_NAME_EXP "^S "


DLSiteHelperServer::DLSiteHelperServer(QObject* parent):Tufao::HttpServer(parent)
{
	//通过HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Services/Tcpip/Parameters/ReservedPorts项将端口设为保留
	listen(QHostAddress::Any, DLConfig::SERVER_PORT);
	connect(this, &DLSiteHelperServer::requestReady, this, &DLSiteHelperServer::HandleRequest);
	SyncLocalFileToDB();
	daily_timer.setInterval(86400*1000);
	daily_timer.start();
	//每天更新本地文件
	connect(&daily_timer, &QTimer::timeout, this, &DLSiteHelperServer::SyncLocalFileToDB);
	//排除本地女性向作品,由于不需要经常使用，直接改代码调用
	//EliminateOTMWorks();
}

DLSiteHelperServer::~DLSiteHelperServer()
{
}

QRegExp DLSiteHelperServer::GetWorkNameExpSep()
{
	return QRegExp(WORK_NAME_EXP_SEP);
}

void DLSiteHelperServer::HandleRequest(Tufao::HttpServerRequest & request, Tufao::HttpServerResponse & response)
{
	QString request_target = request.url().toString();
	QRegExp reg_mark_eliminated("(/\?markEliminated)(" WORK_NAME_EXP ")");
	QRegExp reg_mark_special_eliminated("(/\?markSpecialEliminated)(" WORK_NAME_EXP ")");
	QRegExp reg_mark_overlap("(/\?markOverlap)&main=(" WORK_NAME_EXP ")&sub=(" WORK_NAME_EXP ")&duplex=([0-9])");
	if (request_target.startsWith("/?QueryInvalidDLSite"))
	{
		auto ret = GetAllInvalidWorksString();
		ReplyText(response, Tufao::HttpResponseStatus::OK, ret);
		Log("Response Query Request\n");
	}
	else if (request_target.startsWith("/?QueryOverlapDLSite"))
	{
		auto ret = GetAllOverlapWorksString();
		ReplyText(response, Tufao::HttpResponseStatus::OK, ret);
		Log("Response Query Request\n");
	}
	else if (request_target.startsWith("/?Download"))
	{
		Log("Trying to start download,this may take few minutes");
		SyncLocalFileToDB();
		//user-agent与cookie需要符合，user-agent通过请求的user-agent，cookie通过请求的data获得
		DownloadAll(request.readBody(), request.headers().value("user-agent"));
		ReplyText(response, Tufao::HttpResponseStatus::OK, "Started");
		Log("Response Download Request\n");
	}
	else if (request_target.startsWith("/?RenameLocal")) {
		RenameLocal();
		ReplyText(response, Tufao::HttpResponseStatus::OK, "Started");
		Log("Response Rename Request\n");
	}
	else if (request_target.startsWith("/?UpdateBoughtItems"))
	{
		auto ret = UpdateBoughtItems(request.readBody());
		ReplyText(response, Tufao::HttpResponseStatus::OK, ret);
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
				ReplyText(response, Tufao::HttpResponseStatus::NOT_FOUND, "SQL Fail");
			}
			else
			{
				ReplyText(response, Tufao::HttpResponseStatus::OK, "Success");
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
				ReplyText(response, Tufao::HttpResponseStatus::NOT_FOUND, "SQL Fail");
			}
			else
			{
				ReplyText(response, Tufao::HttpResponseStatus::OK, "Success");
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
			ReplyText(response, Tufao::HttpResponseStatus::NOT_FOUND, "SQL Fail");
		}
		else
		{
			ReplyText(response, Tufao::HttpResponseStatus::OK, "Success");
			Log("Mark Overlap %s%s %s\n", duplex ? "Duplex " : "", main_id.c_str(), sub_id.c_str());
		}
	}
}

void DLSiteHelperServer::ReplyText(Tufao::HttpServerResponse & response,const Tufao::HttpResponseStatus&status, const QString & message)
{
	response.writeHead(Tufao::HttpResponseStatus::OK);
	response.headers().replace("Content-Type", "text/plain");
	response.headers().replace("Access-Control-Allow-Origin", "*");
	response.end(message.toLocal8Bit());
}

QString DLSiteHelperServer::GetAllOverlapWorksString()
{
	auto overlap_work= GetAllOverlapWorks();
	std::string ret="{";
	for (auto& pair : overlap_work)
		if(!pair.second.empty())
		{
			ret += "\"" + pair.first + "\":[";
			for (auto& sub : pair.second)
				ret += "\""+sub+"\",";
			ret.pop_back();//去掉最后一个逗号
			ret += "],";
		}
	if(ret.size()>2)//去掉最后一个逗号
		ret.pop_back();
	ret += "}";
	return s2q(ret);
}

std::map<std::string, std::set<std::string>> DLSiteHelperServer::GetAllOverlapWorks()
{
	DataBase database;
	std::map<std::string, std::set<std::string>> overlap_work;
	//获得手动标记的覆盖关系
	std::map<std::string, std::set<std::string>> base_edges;
	mysql_query(&database.my_sql, "select Main,Sub from overlap;");
	auto result = mysql_store_result(&database.my_sql);
	if (mysql_errno(&database.my_sql))
		LogError("%s\n", mysql_error(&database.my_sql));
	MYSQL_ROW row;
	while (row = mysql_fetch_row(result))
		base_edges[row[0]].insert(row[1]);
	mysql_free_result(result);
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

QString DLSiteHelperServer::UpdateBoughtItems(const QByteArray & data)
{
	DataBase database;
	QRegExp reg(WORK_NAME_EXP);
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

	mysql_query(&database.my_sql, "select count(*) from works where bought=1;");
	auto result = mysql_store_result(&database.my_sql);
	if (mysql_errno(&database.my_sql))
		LogError("%s\n", mysql_error(&database.my_sql));
	int ret = 0;
	MYSQL_ROW row;
	if (row = mysql_fetch_row(result))
		ret = atoi(row[0]);			
	mysql_free_result(result);

	Log("Update Bought List %d\n", ret);
	return "Sucess";
}

QString DLSiteHelperServer::GetAllInvalidWorksString()
{
	std::set<std::string> invalid_work;
	//覆盖的作品全部invalid的作品未必是invalid，因为可能有额外的内容
	//被非invalid覆盖的作品不是invalid，因为可能有合并和分开购买的不同需求	
	{//已下载/标记/购买的作品是invalid
		DataBase database;
		mysql_query(&database.my_sql, "select id from works where eliminated=1 or downloaded=1 or bought=1;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			LogError("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			invalid_work.insert(row[0]);
		mysql_free_result(result);
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
		DataBase database;
		mysql_query(&database.my_sql, "select id from works where specialEliminated=1;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			LogError("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			invalid_work.insert(row[0]);
		mysql_free_result(result);
	}
	std::string ret;
	for(const auto& id:invalid_work)
		ret += (ret.empty() ? "" : " ") + id;
	return s2q(ret);
}

void DLSiteHelperServer::SyncLocalFileToDB()
{
	std::string cmd;
	QRegExp reg(WORK_NAME_EXP);
	DataBase database;
	std::set<std::string> ct;
	std::set<std::string> not_expected_works;//接下来不应该出现在本地的作品
	auto overlaps=GetAllOverlapWorks();
	{//eliminated及其覆盖的作品不应当出现在本地
		std::set<std::string> eliminated_works;
		mysql_query(&database.my_sql, "select id from works where eliminated=1;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			LogError("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			eliminated_works.insert(row[0]);
		mysql_free_result(result);

		not_expected_works = eliminated_works;
		for (auto& i : eliminated_works)
			if (overlaps.count(i))
				for (auto& j : overlaps[i])
					if (!not_expected_works.count(j))
						not_expected_works.insert(j);
	}
	//重置所有作品downloaded状态
	cmd = "UPDATE works set downloaded=0;";
	//依序遍历loacl_dirs,查找已下载的作品，如果下载了多个互相覆盖的作品，保留靠前的目录中的文件
	for (auto& root_dir : DLConfig::local_dirs)
		for (auto& dir : GetLocalFiles(QStringList{ root_dir }))
		{
			int pos = reg.indexIn(QFileInfo(dir).fileName());
			if (pos > -1) {
				std::string work_name = q2s(reg.cap(0));
				if (not_expected_works.count(work_name))//删除不需要的
				{
					//downloaded最开始已经设成0了，此处不需要set
					if (!QDir(dir).removeRecursively())
						Log("Can't Remove %s\n", q2s(dir).c_str());
					else
						Log("Remove Eliminated: %s\n", q2s(dir).c_str());
				}
				else
				{
					//因为已经有该作品了，接下来的遍历中该作品及其覆盖作品都不再需要，如果遇到就删除
					not_expected_works.insert(work_name);
					if (overlaps.count(work_name))
						for (auto& j : overlaps[work_name])
							if (!not_expected_works.count(j))
								not_expected_works.insert(j);

					cmd += "INSERT IGNORE INTO works(id) VALUES(\"" + work_name + "\");"
						"UPDATE works SET downloaded = 1 WHERE id = \"" + work_name + "\"; ";
					ct.insert(work_name);
				}
			}
			if (cmd.length() > SQL_LENGTH_LIMIT)
				database.SendQuery(cmd);
		}
	if (cmd.length() > 0)
		database.SendQuery(cmd);

	mysql_query(&database.my_sql, "select count(*) from works where downloaded=1;");
	auto result = mysql_store_result(&database.my_sql);
	if (mysql_errno(&database.my_sql))
		LogError("%s\n", mysql_error(&database.my_sql));
	int ret = 0;
	MYSQL_ROW row;
	if (row = mysql_fetch_row(result))
		ret = atoi(row[0]);
	mysql_free_result(result);

	Log("Sync from local %zd works->%d\n", ct.size(),ret);
}

void DLSiteHelperServer::DownloadAll(const QByteArray&cookie, const QByteArray& user_agent)
{
	QStringList works;
	{
		DataBase database;
		mysql_query(&database.my_sql, "select id from works where downloaded=0 and bought=1 and eliminated=0;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			LogError("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			works.push_back(QString::fromLocal8Bit(row[0]));
		mysql_free_result(result);
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
	QRegExp regex_is_work(WORK_NAME_EXP);
	QRegExp regex_is_series(SERIES_NAME_EXP);
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

void DLSiteHelperServer::EliminateOTMWorks() {
	//otomei女性向作品
	auto ids=client.GetOTMWorks(GetLocalFiles(DLConfig::local_dirs));
	std::string cmd = "";
	DataBase database;
	for (auto& id : ids)
	{
		cmd += "UPDATE works SET eliminated = 1 WHERE id = \"" + q2s(id) + "\"; ";//由于只检查了本地文件，此处不需要insert
		if (cmd.length() > SQL_LENGTH_LIMIT)
			database.SendQuery(cmd);
	}
	if (cmd.length() > 0)
		database.SendQuery(cmd);

}
