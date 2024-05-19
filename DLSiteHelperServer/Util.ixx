module;
#include <QString>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>
#include "cpr/cpr.h"
import DLConfig;
export module Util;
/*
* 使用import std之后，该模块和import该模块的文件(如果调用了对应函数)都不能直接或间接#include某些std库(包括string和QString等)，否则会出现重定义错误无法编译
* 似乎是因为VS2022对C++20的支持不完全:https://stackoverflow.com/questions/77220989/is-import-std-a-realistic-goal-for-a-c-project-with-transitive-standard-lib
* 因此避免使用import std;
*/
import <cstdarg>;
import <cstdio>;
import <string>;
import <set>;

//#define WORK_NAME_EXP "[RVBJ]{2}[0-9]{3,8}"
//#define WORK_NAME_EXP_SEP "([RVBJ]{2})([0-9]{3,8})"
//#define SERIES_NAME_EXP "^S "

//较常作为std::string用的定义为std::string,否则为QString
//一会用QString一会用std::string我好烦啊
export enum WorkType {
	UNKNOWN,//尚未获取到
	VIDEO,//视频
	AUDIO,//音频
	PICTURE,//图片
	PROGRAM,//程序
	OTHER,//获取失败/获取到了但不知道是什么
	CANTDOWNLOAD,//浏览器专用，无法下载
	SHIT//女性向
};
export struct Task {
	std::string id;//RJ号
	WorkType type = UNKNOWN;//作品的类型
	std::set<std::string> download_ext;//下载的文件(解压前)的格式
	std::vector<std::string> urls;
	std::vector<std::string> cookies;
	std::vector<std::string> file_names;//下载的文件的文件名
	QString work_name;//作品名
	bool ready = false;
	std::string GetDownloadDir() const;
	std::string GetDownloadPath(int i) const;
	std::string GetExtractPath() const;
};
std::string Task::GetDownloadDir() const
{
	std::string path = DLConfig::DOWNLOAD_DIR.toLocal8Bit().toStdString();
	switch (type) {
	case WorkType::AUDIO:path += "ASMR/"; break;
	case WorkType::VIDEO:path += "Video/"; break;
	case WorkType::PICTURE:path += "CG/"; break;
	case WorkType::PROGRAM:path += "Game/"; break;
	default:path += "Default/";
	}
	//zip的解压完还需要转码，所以要和rar区分开
	if (download_ext.count("zip"))
		path += "zip/";
	else if (download_ext.count("rar"))
		path += "rar/";
	else
		path += "other/";
	return path;
}
std::string Task::GetDownloadPath(int i) const
{
	return GetDownloadDir() + file_names[i];
}
std::string Task::GetExtractPath() const
{
	std::string path = DLConfig::DOWNLOAD_DIR.toLocal8Bit().toStdString() + "Extract/";
	switch (type) {
	case WorkType::AUDIO:path += "ASMR/"; break;
	case WorkType::VIDEO:path += "Video/"; break;
	case WorkType::PICTURE:path += "CG/"; break;
	case WorkType::PROGRAM:path += "Game/"; break;
	default:path += "Default/";
	}
	path += id;
	return path;
}

namespace Util {
	//把字母和数字分别capture的表达式
	export const std::string WORK_NAME_EXP_SEP = "([RVBJ]{2})([0-9]{3,8})";
	export const std::string SERIES_NAME_EXP = "^S ";
	export const std::string WORK_NAME_EXP = "[RVBJ]{2}[0-9]{3,8}";
	export void LogError(const char* _Format, ...)
	{
		char buff[1024 * 24] = { 0 };
		va_list args;
		va_start(args, _Format);
		vsprintf_s(buff, sizeof(buff), _Format, args);
		va_end(args);
		fprintf(stderr, buff);
	}
	export void Log(const char* _Format, ...)
	{
		char buff[1024 * 24] = { 0 };
		va_list args;
		va_start(args, _Format);
		vsprintf_s(buff, sizeof(buff), _Format, args);
		va_end(args);
		fprintf(stdout, buff);
	}
	export std::string Format(const char* Format, ...)
	{
		char buff[1024 * 24] = { 0 };

		va_list args;
		va_start(args, Format);
		vsprintf_s(buff, sizeof(buff), Format, args);
		va_end(args);

		std::string str(buff);
		return str;
	}
	export std::string q2s(const QString& str)
	{
		return str.toLocal8Bit().toStdString();
	}
	export std::string q2s(const QByteArray& str)
	{
		return str.toStdString();
	}
	export QString s2q(const std::string& str)
	{
		return QString::fromLocal8Bit(str.c_str());
	}

	export QRegExp GetWorkNameExpSep()
	{
		return QRegExp(WORK_NAME_EXP_SEP.c_str());
	}

	export QString GetIDFromDirName(QString dir)
	{
		auto reg = GetWorkNameExpSep();
		reg.indexIn(dir);
		QString id = reg.cap(0);
		//检查id格式，因为可能通过其它来源下载的文件格式不正确(没有补0)
		QString num_postfix = reg.cap(2);
		if (num_postfix.length() % 2 != 0)//奇数位数字的前面补0
		{
			num_postfix = "0" + num_postfix;
			id = reg.cap(1) + num_postfix;
		}
		return id;
	}
	export bool LoadConfigFromFile(QString file_name)
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
		if (root.contains(key) && root.value(key).isArray())
		{
			DLConfig::local_dirs.clear();
			for (auto dir : root.value(key).toArray())
				DLConfig::local_dirs.append(dir.toString());
		}
		key = "local_tmp_dirs";
		if (root.contains(key) && root.value(key).isArray())
		{
			DLConfig::local_tmp_dirs.clear();
			for (auto dir : root.value(key).toArray())
				DLConfig::local_tmp_dirs.append(dir.toString());
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
}
