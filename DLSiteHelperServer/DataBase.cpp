#include "DataBase.h"
DataBase::DataBase()
{
	mysql_init(&my_sql);
	mysql_real_connect(&my_sql, "127.0.0.1","root", "pixivAss",
		"dlsitehelper",4321,NULL, CLIENT_MULTI_STATEMENTS);
}

DataBase::~DataBase()
{
	mysql_close(&my_sql);
}

void DataBase::SendQuery(std::string& cmd)
{
	mysql_query(&my_sql, cmd.c_str());
	do mysql_free_result(mysql_store_result(&my_sql));
	while (!mysql_next_result(&my_sql));
	cmd = "";
	if (mysql_errno(&my_sql))
		printf("%s\n", mysql_error(&my_sql));
}
