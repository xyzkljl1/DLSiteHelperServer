#include "DLSiteClient.h"
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QVariant>
#include <QList>
#include <QSet>
Q_DECLARE_METATYPE(QList<QNetworkCookie>)
DLSiteClient::DLSiteClient()
{
	QNetworkProxy proxy(QNetworkProxy::ProxyType::HttpProxy, "127.0.0.1", 8000);
	manager.setNetworkAccessible(QNetworkAccessManager::Accessible);
	manager.setProxy(proxy);
	connect(this, &DLSiteClient::statusFinished, this, &DLSiteClient::onStatusFinished);
}


DLSiteClient::~DLSiteClient()
{
}

void DLSiteClient::onStatusFinished()
{
	int ct_f = 0,ct_s=0;
	for (auto& s : status)
	{
		if (s.second.failed)
			ct_f++;
		else if (s.second.ready)
			ct_s++;
	}
	if (ct_f + ct_s == status.size())
	{
		printf("finished");
		FILE* fp = fopen("E:\\ChromeExt\\DLSiteHelperServer\\tmp.txt", "w");
		for (auto& s : status)
		{
			fprintf(fp,"%s %d ",s.first.c_str(),s.second.type);
			for (auto&url : s.second.urls)
				fprintf(fp, "%s ", url.c_str());
			fprintf(fp,"\n");
		}
		fclose(fp);
	}
}

void DLSiteClient::onReceiveType(QNetworkReply* res,std::string id)
{
	if (res->error())
	{
		qDebug() << res->error();
		qDebug() << res->errorString();
	}
	else
	{
		/*
		<div class="work_genre">
		<a href="https://www.dlsite.com/maniax/fsr/=/work_category%5B0%5D/doujin/file_type/IJP/from/icon.work">
		<span class="icon_IJP" title="JPEG">JPEG</span></a>
		<a href="https://www.dlsite.com/maniax/fsr/=/work_category%5B0%5D/doujin/options/WPD/from/icon.work">
		<span class="icon_WPD" title="PDF同捆">PDF同捆</span>
		</a>
		</div>*/
		QString page=QString::fromLocal8Bit(res->readAll());
		int code=res->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (code == 200)
		{
			QSet<QString> types;
			QRegExp reg("class=\"icon_([A-Z]{1,10})\"");
			int cursor = 0;
			int start=-1;
			//找到所有work_genre里的类型图标
			while ((start = page.indexOf("class=\"work_genre\">", cursor)) > 0)
			{
				start += std::string("class=\"work_genre\"").size();				
				int end=page.indexOf("</div>", start);
				QString div = page.mid(start, end-start);
				int div_cursor = 0;
				while ((div_cursor=reg.indexIn(div, div_cursor)) > 0)
				{
					types.insert(reg.cap(1));
					div_cursor += 6;
				}
				cursor = end;
			}
			//优先根据文件形式判断，其次根据作品类型判断
			//因为有的作品格式跟一般的不一致
			if (types.contains("EXE"))//文件形式:软件
				status[id].type = WorkType::PROGRAM;
			else if (types.contains("MP4"))//文件形式:MP4
				status[id].type = WorkType::VIDEO;
			else if(types.contains("MP3") ||types.contains("WAV"))//文件形式:MP3、WAV
				status[id].type = WorkType::AUDIO;
			else if (types.contains("IJP")|| types.contains("PNG") || types.contains("IBP")|| types.contains("HTI"))//文件形式:JPEG、BMP、HTML+图片
				status[id].type = WorkType::PICTURE;
			else if (types.contains("SOU") || types.contains("MUS"))//作品类型:音声、音乐(其它类型的作品含有音乐是MS2)
				status[id].type = WorkType::AUDIO;
			else if (types.contains("RPG") || types.contains("ACN") || types.contains("STG") || types.contains("SLN") || types.contains("ADV")
				|| types.contains("DNV") || types.contains("PZL") || types.contains("TYP") || types.contains("QIZ")
				|| types.contains("TBL") || types.contains("ETC"))
				//作品类型:RPG、动作、射击、模拟、冒险、电子小说、益智(神tmR18益智)、打字、解谜、桌面、其它
				status[id].type = WorkType::PROGRAM;
			else if (types.contains("MOV"))//作品类型:视频
				status[id].type = WorkType::VIDEO;
			else if (types.contains("MNG") || types.contains("ICG"))//作品类型:漫画、CG
				status[id].type = WorkType::PICTURE;
			else if (types.contains("WPD")||types.contains("PDF"))//文件形式:PDF同捆、PDF，无法确定类型
				status[id].type = WorkType::OTHER;
			else if (types.contains("ET3") )//作品类型:其它,无法确定类型
				status[id].type = WorkType::OTHER;
			else
			{
				status[id].type = WorkType::OTHER;
				printf("%s Cant identify type:",id.c_str());
				qDebug() << types;;
			}
		}
		else
			status[id].type = WorkType::OTHER;
	}
	res->deleteLater();
}

void DLSiteClient::onReceiveDownloadTry(QNetworkReply* res, std::string id,int idx)
{	
	int code = res->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (code == 200)
	{
		auto type=res->rawHeader("Content-Type");
		if (type.contains("application/zip")||type.contains("binary/octet-stream")||type.contains("application/octet-stream")|| type.contains("application/octet-stream")||type.contains("application/x-msdownload")
			||type.contains("application/x-rar-compressed"))
		{
			if (idx==0)//单段下载
			{
				status[id].urls.push_back("https://www.dlsite.com/maniax/download/=/product_id/" + id + ".html");
				status[id].ready = true;
				emit statusFinished();
			}
			else//分段下载其中的一段
			{
				status[id].urls.push_back("https://www.dlsite.com/maniax/download/=/number/"+std::to_string(idx)+"/product_id/"+id+".html");
				QNetworkRequest request(QUrl(std::string("https://www.dlsite.com/maniax/download/=/number/"+std::to_string(idx+1)+"/product_id/" + id + ".html").c_str()));
				request.setRawHeader("cookie", cookies);
				request.setRawHeader("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36");
				request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
				auto reply = manager.head(request);
				connect(reply, &QNetworkReply::finished, this, std::bind(&DLSiteClient::onReceiveDownloadTry, this, reply,id, idx+1));
			}
		}
		else if (type.contains("text/html"))
		{
			if (idx==0)//无法单段下载，开始分段尝试
			{
				QNetworkRequest request(QUrl(std::string("https://www.dlsite.com/maniax/download/=/number/1/product_id/"+id+".html").c_str()));
				request.setRawHeader("cookie", cookies);
				request.setRawHeader("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36");
				request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
				auto reply = manager.head(request);
				connect(reply, &QNetworkReply::finished, this, std::bind(&DLSiteClient::onReceiveDownloadTry, this, reply, id,1));
			}
			else
			{
				status[id].failed = true;
				emit statusFinished();
				printf("Receive %s Error:", id.c_str());
			}
		}
		else
		{
			status[id].failed = true;
			emit statusFinished();
			printf("Receive %s Error:", id.c_str());
			qDebug() << type;
		}
	}
	else if (code == 404 && idx>1)//分段下载终了
	{
		status[id].ready = true;
		emit statusFinished();
	}
	else
	{
		status[id].failed = true;
		emit statusFinished();
		printf("Receive %s Error:", id.c_str());
	}
	res->deleteLater();
}


void DLSiteClient::start(const QByteArray& _cookies, const std::vector<std::string>& works)
{
	if (this->running)
		return;
	//因为都是在主线程运行，所以这里不需要用原子操作
	running = true;
	this->cookies = _cookies;
	status.clear();
	for (const auto& id : works)
	{
		//获取类型
		{
			//https://play.dlsite.com/api/dlsite/download?workno=RJ296230
			QNetworkRequest request(QUrl(std::string("https://www.dlsite.com/maniax/work/=/product_id/"+id+".html").c_str()));
			request.setRawHeader("cookie", cookies);
			request.setRawHeader("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36");
			request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
			auto reply=manager.get(request);
			connect(reply, &QNetworkReply::finished, this, std::bind(&DLSiteClient::onReceiveType,this,reply,id));
		}
		{
			QNetworkRequest request(QUrl(std::string("https://www.dlsite.com/maniax/download/=/product_id/"+id+".html").c_str()));
			request.setRawHeader("cookie", cookies);
			request.setRawHeader("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36");
			request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
			auto reply = manager.head(request);
			connect(reply, &QNetworkReply::finished, this, std::bind(&DLSiteClient::onReceiveDownloadTry, this, reply, id,0));
		}
		status[id];
	}
}
