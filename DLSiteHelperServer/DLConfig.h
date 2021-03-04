#pragma once
#include <QStringList>
class DLConfig
{
public:
	//存储目录，位于local_dirs的文件夹视作已下载
	static const QStringList DLConfig::local_dirs;
	//位于local_tmp_dirs的文件仅用于重命名
	static const QStringList DLConfig::local_tmp_dirs;
	//下载目录,新任务会被下载到这个目录
	static const std::string DLConfig::DOWNLOAD_DIR;
	//监听端口，需和DLSiteHelper一致
	static const int SERVER_PORT = 4567;
	//数据库端口
	static const int DATABASE_PORT = 4321;
};

