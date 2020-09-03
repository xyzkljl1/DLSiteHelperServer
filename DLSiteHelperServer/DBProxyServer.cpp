#include "DBProxyServer.h"
#include "DLSiteClient.h"
#include <boost/version.hpp>
#include <boost/config.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/http/reader/request.hpp>
#include <boost/http/reader/response.hpp>
#include <QTcpSocket>
#include <QRegExp>
#include <QDir>
#include <iostream>
#include <string>
#include <map>
#include <set>
//必须放在boost后面
#include "DataBase.h"

const int SQL_LENGTH_LIMIT = 10000;
#define WORK_NAME_EXP "[RVBJ]{1,2}[0-9]{3,6}"

DBProxyServer::DBProxyServer()
{
	connect(this, &DBProxyServer::newConnection, this, &DBProxyServer::onConnected);
	if (!listen(QHostAddress::LocalHost, 4567))
	{
		printf("Cant listen\n");
		exit(-1);
	}
	SyncLocalFile();
}

DBProxyServer::~DBProxyServer()
{
}

void DBProxyServer::onConnected()
{
	auto conn = nextPendingConnection();
	connect(conn, &QTcpSocket::readyRead, this, std::bind(&DBProxyServer::onReceived, this, conn));
	connect(conn, &QTcpSocket::disconnected, this, std::bind(&DBProxyServer::onReleased, this, conn));
}

void DBProxyServer::onReceived(QTcpSocket * conn)
{
	auto byte_array=conn->readAll();
	boost::asio::const_buffer buffer(byte_array.data(),byte_array.size());
	boost::http::reader::request reader;
	reader.set_buffer(buffer);
	std::string method,host, request_target;
	std::string att_name;
	std::size_t nparsed = 0;
	QByteArray data;
	while (reader.code() != boost::http::token::code::end_of_message&&reader.code()!= boost::http::token::code::error_insufficient_data) {
		switch (reader.code()) {
		case boost::http::token::code::skip:break;
		case boost::http::token::code::method:
			method = reader.value<boost::http::token::method>().to_string();
			break;
		case boost::http::token::code::request_target:
			request_target = reader.value<boost::http::token::request_target>().to_string();
			break;
		case boost::http::token::code::field_name:
		case boost::http::token::code::trailer_name:
			att_name = reader.value<boost::http::token::field_name>().to_string();
			break;
		case boost::http::token::code::field_value:
		case boost::http::token::code::trailer_value:
			if (att_name == "Host")
				host = reader.value<boost::http::token::field_value>().to_string();
			att_name = "";
			break;
		case boost::http::token::code::body_chunk:
			data = byte_array.mid(nparsed,reader.token_size());
			break;
		}
		nparsed += reader.token_size();
		reader.next();
	}
	nparsed += reader.token_size();
	reader.next();
	
	if (reader.code() == boost::http::token::code::error_insufficient_data)
	{
		QRegExp reg_mark_eliminated("(/\?markEliminated)(" WORK_NAME_EXP ")");
		QRegExp reg_mark_special_eliminated("(/\?markSpecialEliminated)(" WORK_NAME_EXP ")");
		QRegExp reg_mark_overlap("(/\?markOverlap)&main=(" WORK_NAME_EXP ")&sub=(" WORK_NAME_EXP ")&duplex=([0-9])");
		QString tmp = QString::fromLocal8Bit(request_target.c_str());
		if (request_target.find("/?QueryInvalidDLSite")==0)
		{
			std::string ret = GetAllInvalidWork();
			sendStandardResponse(conn, ret);
			printf("Response Query Request\n");
		}
		else if (request_target.find("/?QueryOverlapDLSite")==0)
		{
			std::string ret = GetAllOverlapWork();
			sendStandardResponse(conn, ret);
			printf("Response Query Request\n");
		}
		else if (request_target.find("/?Download")==0)
		{
			DownloadAll(data);
			sendStandardResponse(conn, "Started");
			printf("Response Download Request\n");
		}
		else if (request_target.find("/?UpdateBoughtItems")==0)
		{
			std::string ret = UpdateBoughtItems(data);
			sendStandardResponse(conn,ret);
			printf("Update Bought Items\n");
		}
		else if (reg_mark_eliminated.indexIn(tmp)>0)
		{
			if (!reg_mark_eliminated.cap(2).isEmpty())
			{
				std::string id = reg_mark_eliminated.cap(2).toLocal8Bit().toStdString();
				DataBase database;
				std::string  cmd = "INSERT IGNORE INTO works(id) VALUES(\"" + id + "\");"
					"UPDATE works SET eliminated = 1 WHERE id = \"" + id + "\"; ";
				mysql_query(&database.my_sql, cmd.c_str());
				if (mysql_errno(&database.my_sql))
				{
					printf("%s\n", mysql_error(&database.my_sql));
					sendFailResponse(conn,"SQL Fail");
				}
				else
				{
					sendStandardResponse(conn, "Sucess");
					printf("Mark %s Eliminated\n", id.c_str());
				}
			}
		}
		else if (reg_mark_special_eliminated.indexIn(tmp) > 0)
		{
			if (!reg_mark_special_eliminated.cap(2).isEmpty())
			{
				std::string id = reg_mark_special_eliminated.cap(2).toLocal8Bit().toStdString();
				DataBase database;
				std::string  cmd = "INSERT IGNORE INTO works(id) VALUES(\"" + id + "\");"
					"UPDATE works SET specialEliminated = 1 WHERE id = \"" + id + "\"; ";
				mysql_query(&database.my_sql, cmd.c_str());
				if (mysql_errno(&database.my_sql))
				{
					printf("%s\n", mysql_error(&database.my_sql));
					sendFailResponse(conn, "SQL Fail");
				}
				else
				{
					sendStandardResponse(conn, "Sucess");
					printf("Mark %s Special Eliminated\n", id.c_str());
				}
			}
		}
		else if (reg_mark_overlap.indexIn(tmp) > 0)
		{
			std::string main_id = reg_mark_overlap.cap(2).toLocal8Bit().toStdString();
			std::string sub_id = reg_mark_overlap.cap(3).toLocal8Bit().toStdString();
			bool duplex = reg_mark_overlap.cap(4).toInt();
			DataBase database;
			std::string cmd = "INSERT IGNORE INTO works(id) VALUES(\"" + main_id + "\");"
							"INSERT IGNORE INTO works(id) VALUES(\"" + sub_id + "\");"
							"INSERT IGNORE INTO overlap VALUES(\"" + main_id + "\",\"" + sub_id + "\");"
							+(duplex?std::string("INSERT IGNORE INTO overlap VALUES(\"" + sub_id + "\",\"" + main_id + "\");"): std::string());
			mysql_query(&database.my_sql, cmd.c_str());
			if (mysql_errno(&database.my_sql))
			{
				printf("%s\n", mysql_error(&database.my_sql));
				sendFailResponse(conn, "SQL Fail");
			}
			else
			{
				sendStandardResponse(conn, "Sucess");
				printf("Mark Overlap %s%s %s\n", duplex ? "Duplex " : "", main_id.c_str(), sub_id.c_str());
			}
		}
	}
	conn->disconnectFromHost();
}

void DBProxyServer::onReleased(QTcpSocket * conn)
{
	conn->deleteLater();
}

void DBProxyServer::sendFailResponse(QTcpSocket * conn, const std::string& message)
{
	QString response = QString("HTTP/1.1 404\r\n"
		"Cache-Control : private\r\n"
		"Access-Control-Allow-Headers:*\r\n"
		"Content-Length:%1\r\n"
		"Content-Type : text/html; charset = utf-8\r\n\r\n"
		"%2\r\n\r\n").arg(message.size()).arg(QString::fromLocal8Bit(message.c_str()));
	conn->write(response.toLocal8Bit());
	conn->waitForBytesWritten();
}

void DBProxyServer::sendStandardResponse(QTcpSocket * conn, const std::string& message)
{
	QString response = QString("HTTP/1.1 200 OK\r\n"
		"Cache-Control : private\r\n"
		"Access-Control-Allow-Headers:*\r\n"
		"Content-Length:%1\r\n"
		"Content-Type : text/html; charset = utf-8\r\n\r\n"
		"%2\r\n\r\n").arg(message.size()).arg(QString::fromLocal8Bit(message.c_str()));
	conn->write(response.toLocal8Bit());
	conn->waitForBytesWritten();
}
std::string DBProxyServer::GetAllOverlapWork()
{
	DataBase database;
	std::map<std::string,std::set<std::string>> overlap_work;
	mysql_query(&database.my_sql, "select Main,Sub from overlap;");
	auto result = mysql_store_result(&database.my_sql);
	if (mysql_errno(&database.my_sql))
		printf("%s\n", mysql_error(&database.my_sql));
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
	return ret;
}

std::string DBProxyServer::UpdateBoughtItems(const QByteArray & data)
{
	DataBase database;
	QRegExp reg(WORK_NAME_EXP);
	std::string	cmd;
	for (const auto& byte : data.split(' '))
	{
		std::string id = QString(byte).toLocal8Bit().toStdString();
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
		printf("%s\n", mysql_error(&database.my_sql));
	int ret = 0;
	MYSQL_ROW row;
	if (row = mysql_fetch_row(result))
		ret = atoi(row[0]);			
	mysql_free_result(result);

	printf("Update Bought List %d\n", ret);
	return "Sucess";
}

std::string DBProxyServer::GetAllInvalidWork()
{
	std::set<std::string> invalid_work;
	//已下载/标记/购买的作品是invalid
	//SpecialEliminate的作品不会令覆盖的作品变为invalid所以要放在后面加入
	{
		DataBase database;
		mysql_query(&database.my_sql, "select id from works where eliminated=1 or downloaded=1 or bought=1;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			printf("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			invalid_work.insert(row[0]);
		mysql_free_result(result);
	}
	{//被invalid的作品覆盖的作品也是invalid
		DataBase database;
		mysql_query(&database.my_sql, "select Main,Sub from overlap;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			printf("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		//暂时认为没有多层的覆盖关系，即没有合集的合集/三个作品相同但每个作品的描述里只写了与一个重合的情况
		while (row = mysql_fetch_row(result))
			if(invalid_work.count(std::string(row[0])))
				invalid_work.insert(row[1]);
		mysql_free_result(result);
	}
	//覆盖的作品全部invalid的作品未必是invalid，因为可能有额外的内容
	//被非invalid覆盖的作品也是非invalid，因为可能有合并和分开购买的不同需求
	//最后加入specialEliminated
	{
		DataBase database;
		mysql_query(&database.my_sql, "select id from works where specialEliminated=1;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			printf("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			invalid_work.insert(row[0]);
		mysql_free_result(result);
	}
	std::string ret;
	for(const auto& id:invalid_work)
		ret += (ret.empty() ? "" : " ") + id;
	return ret;
}

void DBProxyServer::SyncLocalFile()
{
	QStringList local_files;
	for (auto&dir : local_dirs)
		local_files.append(QDir(dir).entryList({ "*" }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name));
	std::string cmd;
	QRegExp reg(WORK_NAME_EXP);
	DataBase database;
	std::set<std::string> ct;
	for (auto& dir : local_files)
	{
		int pos = reg.indexIn(dir);
		if (pos > -1) {
			std::string work_name = reg.cap(0).toLocal8Bit().toStdString();
			cmd +="INSERT IGNORE INTO works(id) VALUES(\""+work_name+"\");"
				"UPDATE works SET downloaded = 1 WHERE id = \""+work_name+"\"; ";
			ct.insert(work_name);
		}
		if (cmd.length() > SQL_LENGTH_LIMIT)
			database.SendQuery(cmd);

	}
	if (cmd.length() > 0)
		database.SendQuery(cmd);

	mysql_query(&database.my_sql, "select count(*) from works where downloaded=1;");
	auto result = mysql_store_result(&database.my_sql);
	if (mysql_errno(&database.my_sql))
		printf("%s\n", mysql_error(&database.my_sql));
	int ret = 0;
	MYSQL_ROW row;
	if (row = mysql_fetch_row(result))
		ret = atoi(row[0]);
	mysql_free_result(result);

	printf("Sync from local %d works->%d\n", ct.size(),ret);
}

void DBProxyServer::DownloadAll(const QByteArray&cookie)
{
	std::vector<std::string> works;
	{
		DataBase database;
		mysql_query(&database.my_sql, "select id from works where downloaded=0 and bought=1 and eliminated=0;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			printf("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			works.push_back(row[0]);
		mysql_free_result(result);
	}
	client.StartDownload(cookie,works);
}