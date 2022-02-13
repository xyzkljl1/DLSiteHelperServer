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
#define WORK_NAME_EXP "[RVBJ]{2}[0-9]{3,6}"

DLSiteHelperServer::DLSiteHelperServer(QObject* parent):Tufao::HttpServer(parent)
{
	//通过HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Services/Tcpip/Parameters/ReservedPorts项将端口设为保留
	listen(QHostAddress::Any, DLConfig::SERVER_PORT);
	connect(this, &DLSiteHelperServer::requestReady, this, &DLSiteHelperServer::HandleRequest);
	SyncLocalFile();
	daily_timer.setInterval(86400*1000);
	daily_timer.start();
	//每天更新本地文件
	connect(&daily_timer, &QTimer::timeout, this, &DLSiteHelperServer::SyncLocalFile);
}

DLSiteHelperServer::~DLSiteHelperServer()
{
}

QRegExp DLSiteHelperServer::GetWorkNameExp()
{
	return QRegExp(WORK_NAME_EXP);
}

void DLSiteHelperServer::HandleRequest(Tufao::HttpServerRequest & request, Tufao::HttpServerResponse & response)
{
	QString request_target = request.url().toString();
	QRegExp reg_mark_eliminated("(/\?markEliminated)(" WORK_NAME_EXP ")");
	QRegExp reg_mark_special_eliminated("(/\?markSpecialEliminated)(" WORK_NAME_EXP ")");
	QRegExp reg_mark_overlap("(/\?markOverlap)&main=(" WORK_NAME_EXP ")&sub=(" WORK_NAME_EXP ")&duplex=([0-9])");
	if (request_target.startsWith("/?QueryInvalidDLSite"))
	{
		auto ret = GetAllInvalidWork();
		ReplyText(response, Tufao::HttpResponseStatus::OK, ret);
		Log("Response Query Request\n");
	}
	else if (request_target.startsWith("/?QueryOverlapDLSite"))
	{
		auto ret = GetAllOverlapWork();
		ReplyText(response, Tufao::HttpResponseStatus::OK, ret);
		Log("Response Query Request\n");
	}
	else if (request_target.startsWith("/?Download"))
	{
		SyncLocalFile();
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
	response.end(message.toLocal8Bit());
}

QString DLSiteHelperServer::GetAllOverlapWork()
{
	DataBase database;
	std::map<std::string,std::set<std::string>> overlap_work;
	mysql_query(&database.my_sql, "select Main,Sub from overlap;");
	auto result = mysql_store_result(&database.my_sql);
	if (mysql_errno(&database.my_sql))
		LogError("%s\n", mysql_error(&database.my_sql));
	MYSQL_ROW row;
	//暂时认为没有多层的覆盖关系，即没有合集的合集/三个作品相同但每个作品的描述里只写了与一个重合的情况
	while (row = mysql_fetch_row(result))
		overlap_work[row[0]].insert(row[1]);
	mysql_free_result(result);
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

QString DLSiteHelperServer::GetAllInvalidWork()
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
		DataBase database;
		mysql_query(&database.my_sql, "select Main,Sub from overlap;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			LogError("%s\n", mysql_error(&database.my_sql));
		//覆盖关系是有多层的，例如一个作品有中日英版，中英版的介绍里各自只写了与日文版相通，然后买了中文版，就构成了一个链条
		std::vector<std::pair<std::string, std::string>> relationship;
		{
			MYSQL_ROW row;
			while (row = mysql_fetch_row(result))
				relationship.push_back({ row[0],row[1] });
			mysql_free_result(result);
		}
		int tmp = 0;
		do//一直循环到找不到新的覆盖关系为止
		{
			tmp = 0;
			for (auto & pair : relationship)
				if (invalid_work.count(pair.first) && !invalid_work.count(pair.second))
				{
					invalid_work.insert(pair.second);
					tmp++;
				}
		}
		while (tmp);
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

void DLSiteHelperServer::SyncLocalFile()
{
	std::string cmd;
	QRegExp reg(WORK_NAME_EXP);
	DataBase database;
	std::set<std::string> ct;
	std::set<std::string> eliminated_works;
	{
		mysql_query(&database.my_sql, "select id from works where eliminated=1;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			LogError("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			eliminated_works.insert(row[0]);
		mysql_free_result(result);
	}
	cmd = "UPDATE works set downloaded=0;";
	for (auto& parent_dir : DLConfig::local_dirs)
		for(auto& dir_info:QDir(parent_dir).entryInfoList({ "*" }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
		{			
			int pos = reg.indexIn(dir_info.fileName());
			if (pos > -1) {
				std::string work_name = q2s(reg.cap(0));
				if(eliminated_works.count(work_name))//eliminated的无论是否download或bought都删除
				{
					//downloaded最开始已经设成0了，此处不需要set					
					if (!QDir(dir_info.filePath()).removeRecursively())
						Log("Can't Remove %s\n", q2s(dir_info.filePath()).c_str());
					else
						Log("Remove Eliminated: %s\n", q2s(dir_info.filePath()).c_str());
				}
				else
				{
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
	QStringList ret;
	QStringList local_files;
	for (auto&dir : DLConfig::local_dirs+ DLConfig::local_tmp_dirs)
		for(auto& info: QDir(dir).entryInfoList({ "*" }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
			local_files.append(info.filePath());
	QRegExp reg(WORK_NAME_EXP);
	std::set<std::string> ct;
	for (auto& dir : local_files)
	{
		int pos = reg.indexIn(dir);
		if (pos > -1)
			ret.push_back(dir);
	}
	client.StartRename(ret);
}