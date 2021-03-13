#include "Util.h"
#include <stdarg.h>
#include "DLConfig.h"
std::string Format(const char* Format, ...)
{
	char buff[1024 * 24] = { 0 };

	va_list args;
	va_start(args, Format);
	vsprintf_s(buff, sizeof(buff), Format, args);
	va_end(args);

	std::string str(buff);
	return str;
}
std::string q2s(const QString& str)
{
	return str.toLocal8Bit().toStdString();
}
std::string q2s(const QByteArray& str)
{
	return str.toStdString();
}
QString s2q(const std::string& str)
{
	return QString::fromLocal8Bit(str.c_str());
}
std::string Task::GetDownloadDir()
{
	std::string path=DLConfig::DOWNLOAD_DIR.toLocal8Bit().toStdString();
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
std::string Task::GetDownloadPath(int i)
{
	return GetDownloadDir() + file_names[i];
}