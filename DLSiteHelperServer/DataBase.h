#pragma once
#include "mysql.h"
#include <string>
#include <vector>
#include <map>
class DataBase
{
public:
	struct Work {
		std::string id;
		bool eliminated;
		bool downloaded;
		bool bought;
		bool specialEliminated;
		std::string info;
	};
	DataBase();
	~DataBase();
	//参数为1表示true，为0表示false，为-1表示any
	//and=true表示所有条件都需要满足，否则表示只要满足一个
	std::vector<DataBase::Work> SelectWorks(bool and, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);
	std::vector<std::string> SelectWorksId(bool and, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);
	int SelectWorksCount(bool and, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);
	void SetWorksInfo(const std::map<std::string, std::string>& id_info_map);
	//更新work的info(默认该条已存在)
	std::vector<std::string> SelectWorksIdWithoutInfo(int limit=100);
	std::vector<std::pair<std::string, std::string>> SelectOverlaps();
	std::string GetSQLMarkOverlap(const std::string& main, const std::string& sub, bool both);
	std::string GetSQLUpdateWork(const std::string &id,int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);

	//发送之后将cmd清空,抛弃结果
	void SendQuery(std::string& cmd);
	MYSQL my_sql;
private:
	std::string GetWhereClause(bool and,int eliminated, int downloaded, int bought, int specialEliminated);

};

