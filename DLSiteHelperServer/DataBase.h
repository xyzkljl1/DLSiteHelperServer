#pragma once
#include "mysql.h"
#include <string>
class DataBase
{
public:
	DataBase();
	~DataBase();
	//����֮��cmd���,�������
	void SendQuery(std::string& cmd);
	MYSQL my_sql;
};

