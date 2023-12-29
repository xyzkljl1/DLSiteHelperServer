#pragma once
#include "mysql.h"
#include <string>
#include <vector>
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
	//����Ϊ1��ʾtrue��Ϊ0��ʾfalse��Ϊ-1��ʾany
	//and=true��ʾ������������Ҫ���㣬�����ʾֻҪ����һ��
	std::vector<DataBase::Work> SelectWorks(bool and, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);
	std::vector<std::string> SelectWorksId(bool and, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);
	int SelectWorksCount(bool and, int eliminated = -1, int downloaded = -1, int bought = -1, int specialEliminated = -1);

	std::vector<std::pair<std::string, std::string>> SelectOverlaps();
	//����֮��cmd���,�������
	void SendQuery(std::string& cmd);
	MYSQL my_sql;
private:
	std::string GetWhereClause(bool and,int eliminated, int downloaded, int bought, int specialEliminated);

};

