module;
#include "mysql.h"
//此文件不能是ixx不然会被默认当成接口单元
//使用module DataBase而不export就会作为Database模块的一部分被编译,此处隐式import了Database因此DataBase.ixx中的内容对此文件都可见
module DataBase;
import DLConfig;
import Util;
using namespace Util;
DataBase::DataBase()
{
	//ImplTestFunc();
	mysql_init(&my_sql);
	mysql_real_connect(&my_sql, "127.0.0.1", "root", "pixivAss",
		"dlsitehelper", DLConfig::DATABASE_PORT, NULL, CLIENT_MULTI_STATEMENTS);
}

DataBase::~DataBase()
{
	mysql_close(&my_sql);
}
std::string DataBase::GetWhereClause(bool isAnd, int eliminated, int downloaded, int bought, int specialEliminated) {
	std::string where_clause = "";
	std::string con_str = isAnd ? " and " : " or ";
	if (eliminated >= 0)
		where_clause += "eliminated=" + std::to_string(eliminated);
	if (downloaded >= 0)
		where_clause += std::string(where_clause == "" ? "" : con_str) + "downloaded = " + std::to_string(downloaded);
	if (bought >= 0)
		where_clause += std::string(where_clause == "" ? "" : con_str) + "bought = " + std::to_string(bought);
	if (specialEliminated >= 0) [[unlikely]]
		where_clause += std::string(where_clause == "" ? "" : con_str) + "specialEliminated = " + std::to_string(specialEliminated);
	if (where_clause != "")
		where_clause = " where " + where_clause;
	return where_clause;
}

std::vector<DataBase::Work> DataBase::SelectWorks(bool isAnd, int eliminated, int downloaded, int bought, int specialEliminated)
{
	std::vector<DataBase::Work> ret;
	std::string sql = "select id,eliminated,downloaded,bought,specialEliminated,info from works"
		+ GetWhereClause(isAnd, eliminated, downloaded, bought, specialEliminated) + ";";
	mysql_query(&my_sql, sql.c_str());
	auto result = mysql_store_result(&my_sql);
	if (mysql_errno(&my_sql)) [[unlikely]]
		LogError("%s\n", mysql_error(&my_sql));
	MYSQL_ROW row;
	while (row = mysql_fetch_row(result))
	{
		Work work;
		work.id = row[0];
		work.eliminated = atoi(row[1]);
		work.downloaded = atoi(row[2]);
		work.bought = atoi(row[3]);
		work.specialEliminated = atoi(row[4]);
		work.info = row[5];
	}
	mysql_free_result(result);
	return std::vector<DataBase::Work>();
}
/*
// got "sorry: not yet implemented"
//Deducing this 会让函数内部如同static函数，调用成员都需用self.fun()而不能用fun()，也不能用this
std::vector<DataBase::Work> DataBase::SelectWorks(this DataBase& self, bool isAnd, int eliminated, int downloaded, int bought, int specialEliminated)
{
	std::vector<DataBase::Work> ret;
	std::string sql = "select id,eliminated,downloaded,bought,specialEliminated,info from works"
		+ self.GetWhereClause(isAnd, eliminated, downloaded, bought, specialEliminated) + ";";
	mysql_query(&self.my_sql, sql.c_str());
	auto result = mysql_store_result(&self.my_sql);
	if (mysql_errno(&self.my_sql))
		LogError("%s\n", mysql_error(&self.my_sql));
	MYSQL_ROW row;
	while (row = mysql_fetch_row(result))
	{
		Work work;
		work.id = row[0];
		work.eliminated = atoi(row[1]);
		work.downloaded = atoi(row[2]);
		work.bought = atoi(row[3]);
		work.specialEliminated = atoi(row[4]);
		work.info = row[5];
	}
	mysql_free_result(result);
	return std::vector<DataBase::Work>();
}*/

std::vector<std::string> DataBase::SelectWorksId(bool isAnd, int eliminated, int downloaded, int bought, int specialEliminated)
{
	std::vector<std::string> ret;
	std::string sql = "select id from works" + GetWhereClause(isAnd, eliminated, downloaded, bought, specialEliminated) + ";";
	mysql_query(&my_sql, sql.c_str());
	auto result = mysql_store_result(&my_sql);
	if (mysql_errno(&my_sql)) [[unlikely]]
		LogError("%s\n", mysql_error(&my_sql));
	MYSQL_ROW row;
	while (row = mysql_fetch_row(result))
		ret.push_back(row[0]);
	mysql_free_result(result);
	return ret;
}

int DataBase::SelectWorksCount(bool isAnd, int eliminated, int downloaded, int bought, int specialEliminated)
{
	std::string sql = "select count(*) from works" + GetWhereClause(isAnd, eliminated, downloaded, bought, specialEliminated) + ";";
	mysql_query(&my_sql, sql.c_str());
	auto result = mysql_store_result(&my_sql);
	if (mysql_errno(&my_sql)) [[unlikely]]
		LogError("%s\n", mysql_error(&my_sql));
	int ret = 0;
	MYSQL_ROW row;
	if (row = mysql_fetch_row(result))
		ret = atoi(row[0]);
	mysql_free_result(result);
	return ret;
}

std::vector<std::string> DataBase::SelectWorksIdWithoutInfo(int limit)
{
	std::vector<std::string> ret;
	std::string sql = "select id from works where info is null limit " + std::to_string(limit) + ";";
	mysql_query(&my_sql, sql.c_str());
	auto result = mysql_store_result(&my_sql);
	if (mysql_errno(&my_sql)) [[unlikely]]
		LogError("%s\n", mysql_error(&my_sql));
	MYSQL_ROW row;
	while (row = mysql_fetch_row(result))
		ret.push_back(row[0]);
	mysql_free_result(result);
	return ret;
}

std::vector<std::pair<std::string, std::string>> DataBase::SelectOverlaps()
{
	std::vector<std::pair<std::string, std::string>> ret;
	mysql_query(&my_sql, "select Main,Sub from overlap;");
	auto result = mysql_store_result(&my_sql);
	if (mysql_errno(&my_sql)) [[unlikely]]
		LogError("%s\n", mysql_error(&my_sql));
	MYSQL_ROW row;
	while (row = mysql_fetch_row(result))
		ret.push_back({ row[0],row[1] });
	mysql_free_result(result);
	return ret;
}

std::string DataBase::GetSQLMarkOverlap(const std::string& main, const std::string& sub, bool both)
{
	std::string cmd;
	cmd += "INSERT IGNORE INTO works(id) VALUES(\"" + main + "\");"
		"INSERT IGNORE INTO works(id) VALUES(\"" + sub + "\");"
		"INSERT IGNORE INTO overlap VALUES(\"" + main + "\",\"" + sub + "\");";
	if (both)
		cmd += "INSERT IGNORE INTO overlap VALUES(\"" + sub + "\",\"" + main + "\");";
	return cmd;
}

std::string DataBase::GetSQLUpdateWork(const std::string& id, int eliminated, int downloaded, int bought, int specialEliminated)
{
	std::string set_clause = "";
	if (eliminated >= 0)
		set_clause += "eliminated=" + std::to_string(eliminated);
	if (downloaded >= 0)
		set_clause += std::string(set_clause == "" ? "" : ",") + "downloaded = " + std::to_string(downloaded);
	if (bought >= 0)
		set_clause += std::string(set_clause == "" ? "" : ",") + "bought = " + std::to_string(bought);
	if (specialEliminated >= 0) [[unlikely]]
		set_clause += std::string(set_clause == "" ? "" : ",") + "specialEliminated = " + std::to_string(specialEliminated);

	if (set_clause == "")
		return "";
	return "update works set " + set_clause + " where id=\"" + id + "\";";
}

void DataBase::SetWorksInfo(const std::map<std::string, std::string>& id_info_map)
{
	MYSQL_STMT* stmt = mysql_stmt_init(&my_sql);
	std::string sql = "UPDATE works set info=? where id=?;";
	if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length()) != 0) [[unlikely]]
	{
		LogError("database error.");
		std::string err = mysql_error(&my_sql);
		return;
	}
	for (const auto& [id, info] : id_info_map)
	{
		MYSQL_BIND binds[2];
		memset(binds, 0, sizeof(binds));//这个size正确吗？
		binds[0].buffer_type = MYSQL_TYPE_STRING;
		binds[0].buffer = (void*)info.c_str();
		binds[0].buffer_length = info.length();
		binds[1].buffer_type = MYSQL_TYPE_STRING;
		binds[1].buffer = (void*)id.c_str();
		binds[1].buffer_length = id.length();
		mysql_stmt_bind_param(stmt, binds);
		mysql_stmt_execute(stmt);
	}
	mysql_stmt_close(stmt);
}


void DataBase::SendQuery(std::string& cmd)
{
	mysql_query(&my_sql, cmd.c_str());
	do mysql_free_result(mysql_store_result(&my_sql));
	while (!mysql_next_result(&my_sql));
	cmd = "";
	if (mysql_errno(&my_sql)) [[unlikely]]
		LogError("%s\n", mysql_error(&my_sql));
}