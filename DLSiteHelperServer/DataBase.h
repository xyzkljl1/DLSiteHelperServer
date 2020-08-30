#pragma once
#include "mysql.h"
#include <string>
class DataBase
{
public:
	DataBase();
	~DataBase();
	//发送之后将cmd清空,抛弃结果
	void SendQuery(std::string& cmd);
	MYSQL my_sql;
};

