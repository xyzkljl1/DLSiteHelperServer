#include "DLConfig.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "Util.h"
QStringList DLConfig::local_dirs{"/Archive/"};
QStringList DLConfig::local_tmp_dirs{"/Tmp/"};
std::string DLConfig::REQUEST_PROXY="127.0.0.1:8000";
std::string DLConfig::REQUEST_PROXY_TYPE="https";
QString DLConfig::DOWNLOAD_DIR="/Download/";
QString DLConfig::ARIA2_PROXY="127.0.0.1:8000";
std::string DLConfig::ARIA2_SECRET="";
int DLConfig::SERVER_PORT=4567;
int DLConfig::DATABASE_PORT=4321;
int DLConfig::ARIA2_PORT=4319;
bool DLConfig::ARIA2_Mode=true;

bool DLConfig::LoadFromFile(QString file_name)
{
	QFile loadFile(file_name);
	if (!loadFile.open(QIODevice::ReadOnly))
	{
		LogError("Can't Load config.json\n");
		return false;
	}

	QByteArray allData = loadFile.readAll();
	loadFile.close();

	QJsonParseError jsonError;
	QJsonDocument jsonDoc(QJsonDocument::fromJson(allData, &jsonError));

	if (jsonError.error != QJsonParseError::NoError)
	{
		LogError("config.json parse error\n");
		return false;
	}
	auto root = jsonDoc.object();
	QString key = "local_dirs";
	if (root.contains(key)&&root.value(key).isArray())
	{
		DLConfig::local_dirs.clear();
		for (auto dir : root.value(key).toArray())
			local_dirs.append(dir.toString());
	}
	key = "local_tmp_dirs";
	if (root.contains(key) && root.value(key).isArray())
	{
		DLConfig::local_tmp_dirs.clear();
		for (auto dir : root.value(key).toArray())
			local_tmp_dirs.append(dir.toString());
	}
	key = "DOWNLOAD_DIR";
	if (root.contains(key) && root.value(key).isString())
		DLConfig::DOWNLOAD_DIR = root.value(key).toString();
	key = "REQUEST_PROXY";
	if (root.contains(key) && root.value(key).isString())
		DLConfig::REQUEST_PROXY = root.value(key).toString().toStdString();
	key = "REQUEST_PROXY_TYPE";
	if (root.contains(key) && root.value(key).isString())
		DLConfig::REQUEST_PROXY_TYPE = root.value(key).toString().toStdString();
	key = "ARIA2_PROXY";
	if (root.contains(key) && root.value(key).isString())
		DLConfig::ARIA2_PROXY = root.value(key).toString();
	key = "ARIA2_SECRET";
	if (root.contains(key) && root.value(key).isString())
		DLConfig::ARIA2_SECRET = root.value(key).toString().toStdString();
	key = "SERVER_PORT";
	if (root.contains(key))
		DLConfig::SERVER_PORT = root.value(key).toInt();
	key = "ARIA2_PORT";
	if (root.contains(key))
		DLConfig::ARIA2_PORT = root.value(key).toInt();
	key = "DATABASE_PORT";
	if (root.contains(key))
		DLConfig::DATABASE_PORT = root.value(key).toInt();
/*	key = "DOWNLOADER";
	if (root.contains(key) && root.value(key).isString())
	{
		if (root.value(key).toString().toLower() == "aria2"|| root.value(key).toString().toLower() == "aria2c"|| root.value(key).toString().toLower() == "aria")
		{
			ARIA2_Mode = true;
		}
	}*/
	return true;
}
