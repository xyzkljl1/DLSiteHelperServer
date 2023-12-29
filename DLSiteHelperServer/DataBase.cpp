#include "DataBase.h"
#include "DLConfig.h"
#include "Util.h"
DataBase::DataBase()
{
	mysql_init(&my_sql);
	mysql_real_connect(&my_sql, "127.0.0.1","root", "pixivAss",
		"dlsitehelper",DLConfig::DATABASE_PORT,NULL, CLIENT_MULTI_STATEMENTS);
}

DataBase::~DataBase()
{
	mysql_close(&my_sql);
}
std::string DataBase::GetWhereClause(bool and,int eliminated, int downloaded, int bought, int specialEliminated) {
	std::string where_clause = "";
	std::string con_str = and ? " and " : " or ";
	if (eliminated >= 0)
		where_clause += "eliminated=" + std::to_string(eliminated);
	if (downloaded >= 0)
		where_clause += std::string(where_clause==""?"": con_str) + "downloaded = " + std::to_string(downloaded);
	if (bought >= 0)
		where_clause += std::string(where_clause == "" ? "" : con_str) + "bought = " + std::to_string(bought);
	if (specialEliminated >= 0)
		where_clause += std::string(where_clause == "" ? "" : con_str) + "specialEliminated = " + std::to_string(specialEliminated);
	if (where_clause != "")
		where_clause = " where " + where_clause;
	return where_clause;
}

std::vector<DataBase::Work> DataBase::SelectWorks(bool and,int eliminated, int downloaded, int bought, int specialEliminated)
{
	std::vector<DataBase::Work> ret;
	std::string sql = "select id,eliminated,downloaded,bought,specialEliminated,info from works" 
				+ GetWhereClause(and, eliminated, downloaded, bought, specialEliminated) + ";";
	mysql_query(&my_sql, sql.c_str());
	auto result = mysql_store_result(&my_sql);
	if (mysql_errno(&my_sql))
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

std::vector<std::string> DataBase::SelectWorksId(bool and, int eliminated, int downloaded, int bought, int specialEliminated)
{
	std::vector<std::string> ret;
	std::string sql = "select id from works" + GetWhereClause(and,eliminated, downloaded, bought, specialEliminated) + ";";
	mysql_query(&my_sql, sql.c_str());
	auto result = mysql_store_result(&my_sql);
	if (mysql_errno(&my_sql))
		LogError("%s\n", mysql_error(&my_sql));
	MYSQL_ROW row;
	while (row = mysql_fetch_row(result))
		ret.push_back(row[0]);
	mysql_free_result(result);
	return ret;
}

int DataBase::SelectWorksCount(bool and, int eliminated, int downloaded, int bought, int specialEliminated)
{
	std::string sql = "select count(*) from works"+ GetWhereClause(and, eliminated, downloaded,bought,specialEliminated)+";";
	mysql_query(&my_sql, sql.c_str());
	auto result = mysql_store_result(&my_sql);
	if (mysql_errno(&my_sql))
		LogError("%s\n", mysql_error(&my_sql));
	int ret = 0;
	MYSQL_ROW row;
	if (row = mysql_fetch_row(result))
		ret = atoi(row[0]);
	mysql_free_result(result);
	return ret;
}

std::vector<std::pair<std::string, std::string>> DataBase::SelectOverlaps()
{
	std::vector<std::pair<std::string, std::string>> ret;
	mysql_query(&my_sql, "select Main,Sub from overlap;");
	auto result = mysql_store_result(&my_sql);
	if (mysql_errno(&my_sql))
		LogError("%s\n", mysql_error(&my_sql));
	MYSQL_ROW row;
	while (row = mysql_fetch_row(result))
		ret.push_back({row[0],row[1]});
	mysql_free_result(result);
	return ret;
}

void DataBase::SendQuery(std::string& cmd)
{
	mysql_query(&my_sql, cmd.c_str());
	do mysql_free_result(mysql_store_result(&my_sql));
	while (!mysql_next_result(&my_sql));
	cmd = "";
	if (mysql_errno(&my_sql))
		LogError("%s\n", mysql_error(&my_sql));
}
