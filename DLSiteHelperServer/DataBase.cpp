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
