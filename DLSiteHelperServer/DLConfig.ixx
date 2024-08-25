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
	inline static bool ARIA2_Mode=true;

	//存储目录，位于local_dirs的文件夹视作已下载
	//有序！当不同目录存在相互覆盖的work时，优先保留靠前的目录中的文件
	inline static std::vector<std::string> local_dirs{ "/Archive/" };
	//位于local_tmp_dirs的文件仅用于重命名
	inline static std::vector<std::string> local_tmp_dirs{ "/Tmp/" };
	//下载目录,新任务会被下载到这个目录
	inline static QString DOWNLOAD_DIR= "/Download/";
	//监听端口，需和DLSiteHelper一致
	inline static int SERVER_PORT=4567;
	//数据库端口1
	inline static int DATABASE_PORT=4321;
	inline static std::string REQUEST_PROXY= "127.0.0.1:8000";
	inline static std::string REQUEST_PROXY_TYPE= "https";
	//ARIA2端口和代理
	inline static int ARIA2_PORT= 4319;
	inline static QString ARIA2_PROXY= "127.0.0.1:8000";
	inline static std::string ARIA2_SECRET;
};
