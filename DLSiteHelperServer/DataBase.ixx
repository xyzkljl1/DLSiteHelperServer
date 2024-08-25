module;
#include "mysql.h"
export module DataBase;//export module DataBase的接口单元只能有一个
export import :Impl;
import <string>;//export会产生编译错误，why?
import <vector>;
import <map>;
export class DataBase
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
	[[nodiscard]] std::vector<DataBase::Work> SelectWorks(bool isAnd, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);
	//try "deducing this",not working
	//std::vector<DataBase::Work> SelectWorks(this DataBase& self, bool isAnd, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);
	[[nodiscard]] std::vector<std::string> SelectWorksId(bool isAnd, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);
	[[nodiscard]] int SelectWorksCount(bool isAnd, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);
	void SetWorksInfo(const std::map<std::string, std::string>& id_info_map);
	//更新work的info(默认该条已存在)
	[[nodiscard]] std::vector<std::string> SelectWorksIdWithoutInfo(int limit = 100);
	[[nodiscard]] std::vector<std::pair<std::string, std::string>> SelectOverlaps();
	[[nodiscard]] std::string GetSQLMarkOverlap(const std::string& main, const std::string& sub, bool both);
	[[nodiscard]] std::string GetSQLUpdateWork(const std::string& id, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);

	//发送之后将cmd清空,抛弃结果
	void SendQuery(std::string& cmd);
	MYSQL my_sql;
private:
	std::string GetWhereClause(bool isAnd, int eliminated, int downloaded, int bought, int specialEliminated);
};

