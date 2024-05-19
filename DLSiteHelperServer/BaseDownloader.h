#include "cpr/cpr.h"
#include <QObject>
import Util;
class BaseDownloader:public QObject
{
	Q_OBJECT
public:
	virtual bool StartDownload(const std::vector<Task>& _tasks,const cpr::Cookies& _cookie,const cpr::UserAgent& _user_agent)=0;
signals:
	void signalDownloadDone(std::vector<Task> tasks);
	void signalDownloadAborted();
protected:
	bool CheckFiles(const std::vector<QString>& files);
};