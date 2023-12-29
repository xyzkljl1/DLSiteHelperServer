#include "DLSiteHelperServer.h"
#include "DLSiteClient.h"
#include <QRegExp>
#include <QDir>
#include <QUrl>
#include <string>
#include <map>
#include <set>
#include "DataBase.h"

const int SQL_LENGTH_LIMIT = 10000;
#define WORK_NAME_EXP "[RVBJ]{2}[0-9]{3,8}"
//����ĸ�����ֱַ�capture�ı��ʽ
#define WORK_NAME_EXP_SEP "([RVBJ]{2})([0-9]{3,8})"
#define SERIES_NAME_EXP "^S "


DLSiteHelperServer::DLSiteHelperServer(QObject* parent):Tufao::HttpServer(parent)
{
	//ͨ��HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Services/Tcpip/Parameters/ReservedPorts��˿���Ϊ����
	listen(QHostAddress::Any, DLConfig::SERVER_PORT);
	connect(this, &DLSiteHelperServer::requestReady, this, &DLSiteHelperServer::HandleRequest);
	SyncLocalFileToDB();
	daily_timer.setInterval(86400*1000);
	daily_timer.start();
	//ÿ����±����ļ�
	connect(&daily_timer, &QTimer::timeout, this, &DLSiteHelperServer::SyncLocalFileToDB);
	//�ų�����Ů������Ʒ,���ڲ���Ҫ����ʹ�ã�ֱ�ӸĴ������
	//EliminateOTMWorks();
	//�ų������԰汾��ͬ��ֱ�ӸĴ������
	//EliminateTranslationWorks();
}

DLSiteHelperServer::~DLSiteHelperServer()
{
}

QRegExp DLSiteHelperServer::GetWorkNameExpSep()
{
	return QRegExp(WORK_NAME_EXP_SEP);
}

QString DLSiteHelperServer::GetIDFromDirName(QString dir)
{
	auto reg = GetWorkNameExpSep();
	reg.indexIn(dir);
	QString id = reg.cap(0);
	//���id��ʽ����Ϊ����ͨ��������Դ���ص��ļ���ʽ����ȷ(û�в�0)
	QString num_postfix = reg.cap(2);
	if (num_postfix.length() % 2 != 0)//����λ���ֵ�ǰ�油0
	{
		num_postfix = "0" + num_postfix;
		id = reg.cap(1) + num_postfix;
	}
	return id;
}

void DLSiteHelperServer::HandleRequest(Tufao::HttpServerRequest & request, Tufao::HttpServerResponse & response)
{
	QString request_target = request.url().toString();
	QRegExp reg_mark_eliminated("(/\?markEliminated)(" WORK_NAME_EXP ")");
	QRegExp reg_mark_special_eliminated("(/\?markSpecialEliminated)(" WORK_NAME_EXP ")");
	QRegExp reg_mark_overlap("(/\?markOverlap)&main=(" WORK_NAME_EXP ")&sub=(" WORK_NAME_EXP ")&duplex=([0-9])");
	if (request_target.startsWith("/?QueryInvalidDLSite"))
	{
		auto ret = GetAllInvalidWorksString();
		ReplyText(response, Tufao::HttpResponseStatus::OK, ret);
		Log("Response Query Request\n");
	}
	else if (request_target.startsWith("/?QueryOverlapDLSite"))
	{
		auto ret = GetAllOverlapWorksString();
		ReplyText(response, Tufao::HttpResponseStatus::OK, ret);
		Log("Response Query Request\n");
	}
	else if (request_target.startsWith("/?Download"))
	{
		Log("Trying to start download,this may take few minutes");
		SyncLocalFileToDB();
		//user-agent��cookie��Ҫ���ϣ�user-agentͨ�������user-agent��cookieͨ�������data���
		DownloadAll(request.readBody(), request.headers().value("user-agent"));
		ReplyText(response, Tufao::HttpResponseStatus::OK, "Started");
		Log("Response Download Request\n");
	}
	else if (request_target.startsWith("/?RenameLocal")) {
		RenameLocal();
		ReplyText(response, Tufao::HttpResponseStatus::OK, "Started");
		Log("Response Rename Request\n");
	}
	else if (request_target.startsWith("/?UpdateBoughtItems"))
	{
		auto ret = UpdateBoughtItems(request.readBody());
		ReplyText(response, Tufao::HttpResponseStatus::OK, ret);
		Log("Update Bought Items\n");
	}
	else if (reg_mark_eliminated.indexIn(request_target) > 0)
	{
		if (!reg_mark_eliminated.cap(2).isEmpty())
		{
			std::string id = q2s(reg_mark_eliminated.cap(2));
			DataBase database;
			std::string  cmd = "INSERT IGNORE INTO works(id) VALUES(\"" + id + "\");"
				"UPDATE works SET eliminated = 1 WHERE id = \"" + id + "\"; ";
			mysql_query(&database.my_sql, cmd.c_str());
			if (mysql_errno(&database.my_sql))
			{
				LogError("%s\n", mysql_error(&database.my_sql));
				ReplyText(response, Tufao::HttpResponseStatus::NOT_FOUND, "SQL Fail");
			}
			else
			{
				ReplyText(response, Tufao::HttpResponseStatus::OK, "Success");
				Log("Mark %s Eliminated\n", id.c_str());
			}
		}
	}
	else if (reg_mark_special_eliminated.indexIn(request_target) > 0)
	{
		if (!reg_mark_special_eliminated.cap(2).isEmpty())
		{
			std::string id = q2s(reg_mark_special_eliminated.cap(2));
			DataBase database;
			std::string  cmd = "INSERT IGNORE INTO works(id) VALUES(\"" + id + "\");"
				"UPDATE works SET specialEliminated = 1 WHERE id = \"" + id + "\"; ";
			mysql_query(&database.my_sql, cmd.c_str());
			if (mysql_errno(&database.my_sql))
			{
				LogError("%s\n", mysql_error(&database.my_sql));
				ReplyText(response, Tufao::HttpResponseStatus::NOT_FOUND, "SQL Fail");
			}
			else
			{
				ReplyText(response, Tufao::HttpResponseStatus::OK, "Success");
				Log("Mark %s Special Eliminated\n", id.c_str());
			}
		}
	}
	else if (reg_mark_overlap.indexIn(request_target) > 0)
	{
		std::string main_id = q2s(reg_mark_overlap.cap(2));
		std::string sub_id = q2s(reg_mark_overlap.cap(3));
		bool duplex = reg_mark_overlap.cap(4).toInt();
		DataBase database;
		std::string cmd = "INSERT IGNORE INTO works(id) VALUES(\"" + main_id + "\");"
			"INSERT IGNORE INTO works(id) VALUES(\"" + sub_id + "\");"
			"INSERT IGNORE INTO overlap VALUES(\"" + main_id + "\",\"" + sub_id + "\");"
			+ (duplex ? std::string("INSERT IGNORE INTO overlap VALUES(\"" + sub_id + "\",\"" + main_id + "\");") : std::string());
		mysql_query(&database.my_sql, cmd.c_str());
		if (mysql_errno(&database.my_sql))
		{
			LogError("%s\n", mysql_error(&database.my_sql));
			ReplyText(response, Tufao::HttpResponseStatus::NOT_FOUND, "SQL Fail");
		}
		else
		{
			ReplyText(response, Tufao::HttpResponseStatus::OK, "Success");
			Log("Mark Overlap %s%s %s\n", duplex ? "Duplex " : "", main_id.c_str(), sub_id.c_str());
		}
	}
}

void DLSiteHelperServer::ReplyText(Tufao::HttpServerResponse & response,const Tufao::HttpResponseStatus&status, const QString & message)
{
	response.writeHead(Tufao::HttpResponseStatus::OK);
	response.headers().replace("Content-Type", "text/plain");
	response.headers().replace("Access-Control-Allow-Origin", "*");
	response.end(message.toLocal8Bit());
}

QString DLSiteHelperServer::GetAllOverlapWorksString()
{
	auto overlap_work= GetAllOverlapWorks();
	std::string ret="{";
	for (auto& pair : overlap_work)
		if(!pair.second.empty())
		{
			ret += "\"" + pair.first + "\":[";
			for (auto& sub : pair.second)
				ret += "\""+sub+"\",";
			ret.pop_back();//ȥ�����һ������
			ret += "],";
		}
	if(ret.size()>2)//ȥ�����һ������
		ret.pop_back();
	ret += "}";
	return s2q(ret);
}

std::map<std::string, std::set<std::string>> DLSiteHelperServer::GetAllOverlapWorks()
{
	DataBase database;
	std::map<std::string, std::set<std::string>> overlap_work;
	//����ֶ���ǵĸ��ǹ�ϵ
	std::map<std::string, std::set<std::string>> base_edges;
	mysql_query(&database.my_sql, "select Main,Sub from overlap;");
	auto result = mysql_store_result(&database.my_sql);
	if (mysql_errno(&database.my_sql))
		LogError("%s\n", mysql_error(&database.my_sql));
	MYSQL_ROW row;
	while (row = mysql_fetch_row(result))
		base_edges[row[0]].insert(row[1]);
	mysql_free_result(result);
	//�Ƶ����������ǹ�ϵ
	overlap_work = base_edges;
	for (auto& pair : overlap_work)
	{
		auto& ret = pair.second;
		//����ÿ��work������base_edges����
		std::set<std::string> starts = ret;
		std::set<std::string> next_starts;
		do {
			for (auto& s : starts)
				if (base_edges.count(s))
					for (auto& t : base_edges[s])
						if (t != pair.first && !ret.count(t))
						{
							ret.insert(t);
							next_starts.insert(t);
						}
			starts = next_starts;
			next_starts.clear();
		} while (!starts.empty());
	}
	return overlap_work;
}

QString DLSiteHelperServer::UpdateBoughtItems(const QByteArray & data)
{
	DataBase database;
	QRegExp reg(WORK_NAME_EXP);
	std::string	cmd;
	for (const auto& byte : data.split(' '))
	{
		std::string id = q2s(byte);
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
		LogError("%s\n", mysql_error(&database.my_sql));
	int ret = 0;
	MYSQL_ROW row;
	if (row = mysql_fetch_row(result))
		ret = atoi(row[0]);			
	mysql_free_result(result);

	Log("Update Bought List %d\n", ret);
	return "Sucess";
}

QString DLSiteHelperServer::GetAllInvalidWorksString()
{
	std::set<std::string> invalid_work;
	//���ǵ���Ʒȫ��invalid����Ʒδ����invalid����Ϊ�����ж��������
	//����invalid���ǵ���Ʒ����invalid����Ϊ�����кϲ��ͷֿ�����Ĳ�ͬ����	
	{//������/���/�������Ʒ��invalid
		DataBase database;
		mysql_query(&database.my_sql, "select id from works where eliminated=1 or downloaded=1 or bought=1;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			LogError("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			invalid_work.insert(row[0]);
		mysql_free_result(result);
	}
	{//��invalid��Ʒ���ǵ���ƷҲ��invalid
		auto overlaps = GetAllOverlapWorks();
		auto tmp = invalid_work;
		for (auto& i : tmp)
			if (overlaps.count(i))
				for (auto& j : overlaps[i])
					if (!invalid_work.count(j))
						invalid_work.insert(j);
	}
	{//������specialEliminated��SpecialEliminate����Ʒ������ǵ���Ʒ��Ϊinvalid����Ҫ���ں������
		DataBase database;
		mysql_query(&database.my_sql, "select id from works where specialEliminated=1;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			LogError("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			invalid_work.insert(row[0]);
		mysql_free_result(result);
	}
	std::string ret;
	for(const auto& id:invalid_work)
		ret += (ret.empty() ? "" : " ") + id;
	return s2q(ret);
}

void DLSiteHelperServer::SyncLocalFileToDB()
{
	std::string cmd;
	DataBase database;
	std::set<std::string> overlapped_works;//��������/���ų���Ʒ���ǵ���Ʒ
	std::map<std::string,QString> downloaded_works;//��������Ʒ��id-dir
	auto overlaps=GetAllOverlapWorks();
	{//��ȡeliminated���串�ǵ���Ʒ
		std::set<std::string> eliminated_works;
		mysql_query(&database.my_sql, "select id from works where eliminated=1;");
		auto result = mysql_store_result(&database.my_sql);
		if (mysql_errno(&database.my_sql))
			LogError("%s\n", mysql_error(&database.my_sql));
		MYSQL_ROW row;
		while (row = mysql_fetch_row(result))
			eliminated_works.insert(row[0]);
		mysql_free_result(result);
		overlapped_works = eliminated_works;
		for (auto& i : eliminated_works)
			if (overlaps.count(i))
				for (auto& j : overlaps[i])
					if (!overlapped_works.count(j))
						overlapped_works.insert(j);
	}
	//�������local_dirs,���������ص���Ʒȥ�أ���������˶�����า�ǵ���Ʒ��������ǰ��Ŀ¼�е��ļ�
	for (auto& root_dir : DLConfig::local_dirs)
		for (auto& dir : GetLocalFiles(QStringList{ root_dir }))
		{
			std::string id = q2s(GetIDFromDirName(QFileInfo(dir).fileName()));
			if(id.empty())
				continue;
			if (downloaded_works.count(id) || overlapped_works.count(id))//ɾ������Ҫ��
			{
				//downloaded�ʼ�Ѿ����0�ˣ��˴�����Ҫset
				if (!QDir(dir).removeRecursively())
					Log("Can't Remove %s\n", q2s(dir).c_str());
				else
					Log("Remove Eliminated: %s\n", q2s(dir).c_str());
			}
			else
			{
				downloaded_works[id]=dir;
				//������Ʒ���ǵ���Ʒ
				if (overlaps.count(id))
					for (auto& sub_id : overlaps[id])
					{
						if (sub_id == id)
							continue;
						if (downloaded_works.count(sub_id))//����������ѱ�������Ʒ�����֮ǰ��ɾ��(����Ʒ�����˸�else˵�����߲���˫�򸲸ǣ����Ǹ���Ʒ���򸲸�֮ǰ����Ʒ)
						{
							if (!QDir(downloaded_works[sub_id]).removeRecursively())
								Log("Can't Remove %s\n", q2s(downloaded_works[sub_id]).c_str());
							else
								Log("Remove Eliminated: %s\n", q2s(downloaded_works[sub_id]).c_str());
							downloaded_works.erase(sub_id);
						}
						if(!overlapped_works.count(sub_id))//����overlapped_works
							overlapped_works.insert(sub_id);
					}
			}
		}
	//downloadedд�����ݿ�
	{
		//����������Ʒdownloaded״̬
		cmd = "UPDATE works set downloaded=0;";
		for (const auto& pair : downloaded_works)
		{
			std::string id = pair.first;
			cmd += "INSERT IGNORE INTO works(id) VALUES(\"" + id + "\");"
				"UPDATE works SET downloaded = 1 WHERE id = \"" + id + "\"; ";
			if (cmd.length() > SQL_LENGTH_LIMIT)
				database.SendQuery(cmd);
		}
		if (cmd.length() > 0)
			database.SendQuery(cmd);
	}
	Log("Sync from local %zd works\n", downloaded_works.size());
}

void DLSiteHelperServer::DownloadAll(const QByteArray&cookie, const QByteArray& user_agent)
{
	QStringList works;
	{
		DataBase database;
		std::set<std::string> not_expected_works;//����Ҫ���ص�
		{//eliminated/downloaded���串�ǵ���Ʒ����Ҫ����
			auto overlaps = GetAllOverlapWorks();
			std::set<std::string> eliminated_works;
			mysql_query(&database.my_sql, "select id from works where eliminated=1 or downloaded=1;");
			auto result = mysql_store_result(&database.my_sql);
			if (mysql_errno(&database.my_sql))
				LogError("%s\n", mysql_error(&database.my_sql));
			MYSQL_ROW row;
			while (row = mysql_fetch_row(result))
				eliminated_works.insert(row[0]);
			mysql_free_result(result);
			not_expected_works = eliminated_works;
			for (auto& i : eliminated_works)
				if (overlaps.count(i))
					for (auto& j : overlaps[i])
						if (!not_expected_works.count(j))
							not_expected_works.insert(j);
		}
		{
			mysql_query(&database.my_sql, "select id from works where downloaded=0 and bought=1 and eliminated=0;");
			auto result = mysql_store_result(&database.my_sql);
			if (mysql_errno(&database.my_sql))
				LogError("%s\n", mysql_error(&database.my_sql));
			MYSQL_ROW row;
			while (row = mysql_fetch_row(result))
				if (!not_expected_works.count(row[0]))
					works.push_back(QString::fromLocal8Bit(row[0]));
			mysql_free_result(result);
		}
	}
	client.StartDownload(cookie,user_agent,works);
}

void DLSiteHelperServer::RenameLocal()
{
	client.StartRename(GetLocalFiles(DLConfig::local_dirs + DLConfig::local_tmp_dirs));
}
void DLSiteHelperServer::EliminateTranslationWorks()
{
	DataBase database;
	std::string cmd;
	int ct1 = 0;
	int ct2 = 0;
	for(const auto&file: GetLocalFiles(DLConfig::local_dirs))
		if (QFileInfo(file).fileName().contains(s2q("���İ�")))//�󲿷ַ�����Ʒ���ִ�"���İ�"�������ĺ���
		{
			QString id=DLSiteHelperServer::GetIDFromDirName(file);
			//id = "RJ01018121";
			QStringList translations=client.GetTranslationWorks(id);
			ct1 += translations.count() > 1 ? 1 : 0;
			ct2 += translations.count()>1?translations.count():0;
			//�������Ϊ˫�򸲸�
			for(int i=0;i<translations.count();++i)
				for (int j = i + 1; j < translations.count(); ++j)
				{
					cmd+=q2s("INSERT IGNORE INTO works(id) VALUES(\"" + translations[i] + "\");"
						"INSERT IGNORE INTO works(id) VALUES(\"" + translations[j] + "\");"
						"INSERT IGNORE INTO overlap VALUES(\"" + translations[i] + "\",\"" + translations[j] + "\");"
						"INSERT IGNORE INTO overlap VALUES(\"" + translations[j] + "\",\"" + translations[i] + "\");"
						);
				}
			if (cmd.length() > SQL_LENGTH_LIMIT)
				database.SendQuery(cmd);
		}
	if (cmd.length() > 0)
		database.SendQuery(cmd);
	Log("Eliminate Translations Done.Marked %d group/%d works.",ct1,ct2);
}

//���ָ����Ŀ¼������work��·��
//������Ŀ¼��һ���Լ���Ŀ¼��[��SERIES_NAME_EXP��ͷ��Ŀ¼]����һ��
QStringList DLSiteHelperServer::GetLocalFiles(const QStringList& root)
{
	QStringList ret;
	QRegExp regex_is_work(WORK_NAME_EXP);
	QRegExp regex_is_series(SERIES_NAME_EXP);
	for (auto& dir : root)
		for (auto& info : QDir(dir).entryInfoList({ "*" }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
			if (regex_is_work.indexIn(info.fileName()) > -1)
				ret.push_back(info.filePath());
			else if (regex_is_series.indexIn(info.fileName()) > -1)
			{
				for (auto& sub_info : QDir(info.filePath()).entryInfoList({ "*" }, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
					if (regex_is_work.indexIn(sub_info.fileName()) > -1)
						ret.push_back(sub_info.filePath());
			}
	return ret;
}

void DLSiteHelperServer::EliminateOTMWorks() {
	//otomeiŮ������Ʒ
	auto ids=client.GetOTMWorks(GetLocalFiles(DLConfig::local_dirs));
	std::string cmd = "";
	DataBase database;
	for (auto& id : ids)
	{
		cmd += "UPDATE works SET eliminated = 1 WHERE id = \"" + q2s(id) + "\"; ";//����ֻ����˱����ļ����˴�����Ҫinsert
		if (cmd.length() > SQL_LENGTH_LIMIT)
			database.SendQuery(cmd);
	}
	if (cmd.length() > 0)
		database.SendQuery(cmd);

}
