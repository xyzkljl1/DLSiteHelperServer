#pragma once
#include <string>
#include <QString>
#include <set>
#include <vector>
extern std::string Format(const char * Format, ...);
extern std::string q2s(const QString&);
extern std::string q2s(const QByteArray&);
extern QString s2q(const std::string&);
extern void LogError(const char * Format, ...);
extern void Log(const char * Format, ...);
//较常作为std::string用的定义为std::string,否则为QString
//一会用QString一会用std::string我好烦啊
enum WorkType {
	UNKNOWN,//尚未获取到
	VIDEO,//视频
	AUDIO,//音频
	PICTURE,//图片
	PROGRAM,//程序
	OTHER,//获取失败/获取到了但不知道是什么
	CANTDOWNLOAD,//浏览器专用，无法下载
	SHIT//女性向
};
struct Task {
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
