module;
#include <QStringList>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
export module DLConfig;
export class DLConfig
{
public:
	static bool IDM_Mode;
	static bool ARIA2_Mode;

	//存储目录，位于local_dirs的文件夹视作已下载
	//有序！当不同目录存在相互覆盖的work时，优先保留靠前的目录中的文件
	static QStringList local_dirs;
	//位于local_tmp_dirs的文件仅用于重命名
	static QStringList local_tmp_dirs;
	//下载目录,新任务会被下载到这个目录
	static QString DOWNLOAD_DIR;
	//监听端口，需和DLSiteHelper一致
	static int SERVER_PORT;
	//数据库端口
	static int DATABASE_PORT;
	static std::string REQUEST_PROXY;
	static std::string REQUEST_PROXY_TYPE;
	//ARIA2端口和代理
	static int ARIA2_PORT;
	static QString ARIA2_PROXY;
	static std::string ARIA2_SECRET;
};

QStringList DLConfig::local_dirs{ "/Archive/" };
QStringList DLConfig::local_tmp_dirs{ "/Tmp/" };
std::string DLConfig::REQUEST_PROXY = "127.0.0.1:8000";
std::string DLConfig::REQUEST_PROXY_TYPE = "https";
QString DLConfig::DOWNLOAD_DIR = "/Download/";
QString DLConfig::ARIA2_PROXY = "127.0.0.1:8000";
std::string DLConfig::ARIA2_SECRET = "";
int DLConfig::SERVER_PORT = 4567;
int DLConfig::DATABASE_PORT = 4321;
int DLConfig::ARIA2_PORT = 4319;
bool DLConfig::ARIA2_Mode = true;