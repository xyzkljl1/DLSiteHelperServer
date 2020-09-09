#include "DLSiteClient.h"
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QVariant>
#include <QList>
#include <QTextCodec>
#include <QDir>
#include <QSet>
#import "IDManTypeInfo.tlb" 
#include "IDManTypeInfo.h"            
#include "IDManTypeInfo_i.c"
//#include <atlbase.h>    //for BSTR class
#include <comutil.h>
#include <atlthunk.h>
#include "DBProxyServer.h"
DLSiteClient::DLSiteClient()
{
	QNetworkProxy proxy(QNetworkProxy::ProxyType::HttpProxy, "127.0.0.1", 8000);
	manager.setNetworkAccessible(QNetworkAccessManager::Accessible);
	manager.setProxy(proxy);
	//其实这里用信号没什么意义，完全可以直接调用槽
	connect(this, &DLSiteClient::downloadStatusChanged, this, &DLSiteClient::onDownloadStatusChanged);
	connect(this, &DLSiteClient::renameStatusChanged, this, &DLSiteClient::onRenameStatusChanged);
}

DLSiteClient::~DLSiteClient()
{
}

void DLSiteClient::onRenameStatusChanged()
{
	for (auto& s : status)
		if (s.second.work_name.isEmpty())
			return;
	auto reg = DBProxyServer::GetWorkNameExp();
	int ct = 0;
	for (const auto& file : local_files)
	{
		reg.indexIn(file);
		QString id = reg.cap(0);
		int pos = file.lastIndexOf(QRegExp("[\\/]")) + 1;
		QString old_name = QDir(file).dirName();
		QString new_name = id+" "+status[id].work_name;
		if (old_name == new_name)
			continue;
		QDir parent(file);
		parent.cdUp();
		while (parent.exists(new_name))
			new_name=new_name+"_repeat";
		if (old_name == new_name)
			continue;
		if (!parent.rename(old_name, new_name))
			qDebug() << "Cant Rename " << old_name << "->" << new_name;
		else
			ct ++;
	}
	status.clear();
	running = false;
	printf("Rename %d of %d\n", ct, local_files.size());
}
void DLSiteClient::onDownloadStatusChanged()
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
		SendTaskToIDM();
}
void DLSiteClient::SendTaskToIDM()
{
	ICIDMLinkTransmitter2* pIDM;
	HRESULT hr = CoCreateInstance(CLSID_CIDMLinkTransmitter,
		NULL,
		CLSCTX_LOCAL_SERVER,
		IID_ICIDMLinkTransmitter2,
		(void**)&pIDM);
	int ct = 0;
	int no_download_ct = 0;
	if (S_OK == hr)
		for (const auto& task_pair : status)
		{
			bool success = true;
			auto& task = task_pair.second;
			if (task.type == WorkType::CANTDOWNLOAD)
			{
				no_download_ct++;
				continue;
			}
			if (!task.ready)
				continue;
			if (task.failed)
				continue;
			if (task.urls.empty())
				continue;

			std::string path=DOWNLOAD_DIR;
			switch (task.type) {
			case WorkType::AUDIO:path += "ASMR/"; break;
			case WorkType::VIDEO:path += "Video/"; break;
			case WorkType::PICTURE:path += "CG/"; break;
			case WorkType::PROGRAM:path += "Game/"; break;
			default:path += "Default";
			}
			//zip的解压完还需要转码，所以要和rar区分开
			if (task.download_ext.count("zip"))
				path += "zip/";
			else if (task.download_ext.count("rar"))
				path += "rar/";
			else
				path += "other/";
			if (!QDir(QString::fromLocal8Bit(path.c_str())).exists())
				QDir().mkpath(QString::fromLocal8Bit(path.c_str()));
			for (int i=0;i<task.urls.size();++i)
			{
				VARIANT var;
				VariantInit(&var);
				var.vt = VT_EMPTY;
				_bstr_t url(task.urls[i].c_str());
				_bstr_t fname(task.file_names[i].c_str());
				_bstr_t referer("https://play.dlsite.com/#/library");
				_bstr_t path_b(path.c_str());
				_bstr_t cookies(this->cookies.toStdString().c_str());
				_bstr_t data("");
				_bstr_t user("");
				_bstr_t pass("");
				int flags = 0x01|0x02;//0x01:不确认，0x02:稍后下载

				hr = pIDM->SendLinkToIDM2(url, referer, cookies, data, user, pass, path_b, fname, flags, var, var);

				if (S_OK != hr)
				{
					puts("[-] SendLinkToIDM2 fail!");
					success = false;
				}
			}
			if (success)
				ct++;
		}
	else
		puts("[-] CoCreateInstance fail!");
	pIDM->Release();
	printf("%d in %zd Task Created,$d is not for download\n",ct,status.size()- no_download_ct, no_download_ct);
	this->running = false;
	status.clear();
}

void DLSiteClient::onReceiveType(QNetworkReply* res, QString id)
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
			if (types.contains("WBO"))//文件形式:浏览器专用，不下载 ※最优先
				status[id].type = WorkType::CANTDOWNLOAD;
			else if (types.contains("EXE"))//文件形式:软件
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
			else if (types.contains("WPD")||types.contains("PDF")||types.contains("icon_HTF"))//文件形式:PDF同捆、PDF、HTML(flash)，无法确定类型
				status[id].type = WorkType::OTHER;
			else if (types.contains("AVI") )//文件形式:AVI，无法确定类型
				status[id].type = WorkType::OTHER;
			else if (types.contains("ET3") )//作品类型:其它,无法确定类型
				status[id].type = WorkType::OTHER;
			else
			{
				status[id].type = WorkType::OTHER;
				qDebug() << id<< " Cant identify type:"<< types;
			}
		}
		else
			status[id].type = WorkType::OTHER;
	}
	res->deleteLater();
}

QString DLSiteClient::unicodeToString(const QString& str)
{
	QString result=str;
	int index = result.indexOf("\\u");
	while (index != -1)
	{
		result.replace(result.mid(index, 6),QString(result.mid(index + 2, 4).toUShort(0, 16)).toUtf8());
		index = result.indexOf("\\u");
	}
	return result;
}

void DLSiteClient::onReceiveProductInfo(QNetworkReply *res, QString id)
{
	status[id].work_name = "_";
	if (res->error())
	{
		qDebug() << res->error();
		qDebug() << res->errorString();
	}
	else
	{
		QByteArray doc = res->readAll();
		int code = res->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (code == 200)
		{
			QRegExp reg("\"work_name\"[:\" ]*([^\"]*)\",");
			QString ret;
			if (reg.indexIn(doc) >= 0)
				ret = unicodeToString(reg.cap(1));
			ret.replace(QRegExp("[/\\\\?*<>:\"|.]"),"_");//替换不能/不想出现在文件名中的字符
			status[id].work_name = ret;
		}
	}
	res->deleteLater();
	emit onRenameStatusChanged();
}

void DLSiteClient::onReceiveDownloadTry(QNetworkReply* res,QString id,int idx)
{	
	int code = res->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	if (code == 200)
	{
		auto type=res->rawHeader("Content-Type");
		if (type.contains("application/zip")||type.contains("binary/octet-stream")||type.contains("application/octet-stream")|| type.contains("application/octet-stream")||type.contains("application/x-msdownload")
			||type.contains("application/x-rar-compressed"))
		{
			QString url = res->url().toString();
			std::string ext;
			QString file_name;
			QRegExp reg("filename=\"(.*)\"");
			auto header_disposition = res->rawHeader("Content-Disposition");
			//从文件头中尝试找文件名
			if (reg.indexIn(header_disposition) >= 0)
				file_name = reg.cap(1);
			else//从网址中找
			{
				QRegExp reg("file/(.*)/");
				if (reg.indexIn(url))
					file_name = reg.cap(1);
			}
			int pos = file_name.lastIndexOf('.');
			if (pos >= 0)
				ext = file_name.right(file_name.length() - pos - 1).toStdString();

			status[id].file_names.push_back(file_name.toLocal8Bit().toStdString());
			//这里是重定向的所以要用res的url
			status[id].urls.push_back(url.toLocal8Bit().toStdString());
			if (!ext.empty())
				status[id].download_ext.insert(ext);
			if (idx==0)//单段下载
			{				
				status[id].ready = true;
				emit downloadStatusChanged();
			}
			else//分段下载其中的一段
			{
				QNetworkRequest request(QUrl(QString("https://www.dlsite.com/maniax/download/=/number/%1/product_id/%2.html").arg(idx+1).arg(id)));
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
				QNetworkRequest request(QUrl(QString("https://www.dlsite.com/maniax/download/=/number/1/product_id/%1.html").arg(id)));
				request.setRawHeader("cookie", cookies);
				request.setRawHeader("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36");
				request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
				auto reply = manager.head(request);
				connect(reply, &QNetworkReply::finished, this, std::bind(&DLSiteClient::onReceiveDownloadTry, this, reply, id,1));
			}
			else
			{
				status[id].failed = true;
				emit downloadStatusChanged();
				qDebug() << "Receive "<<id<<" Error:";
			}
		}
		else
		{
			status[id].failed = true;
			emit downloadStatusChanged();
			qDebug() << "Receive "<< id<<" Error:"<<type;
		}
	}
	else if (code == 404 && idx>1)//分段下载终了
	{
		status[id].ready = true;
		emit downloadStatusChanged();
	}
	else
	{
		status[id].failed = true;
		emit downloadStatusChanged();
		qDebug() << "Receive " << id << " Error";
	}
	res->deleteLater();
}

void DLSiteClient::StartDownload(const QByteArray& _cookies, const QStringList& works)
{
	if (this->running)
		return;
	//因为都是在主线程运行，所以这里不需要用原子操作
	running = true;
	this->cookies = _cookies;
	status.clear();
	for (const auto& id : works)
	{
		status[id];
		//url的第一级根据卖场分为manix、pro、books，但是实际可以随便用
		//产品页面的第二级根据是否发售分为work、announce,下载的都是work
		//获取类型
		{
			//https://play.dlsite.com/api/dlsite/download?workno=RJ296230
			QNetworkRequest request(QUrl(QString("https://www.dlsite.com/maniax/work/=/product_id/%1.html").arg(id)));
			request.setRawHeader("cookie", cookies);
			request.setRawHeader("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36");
			request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
			auto reply=manager.get(request);
			connect(reply, &QNetworkReply::finished, this, std::bind(&DLSiteClient::onReceiveType,this,reply,id));
		}
		//获取下载地址
		{
			QNetworkRequest request(QUrl(QString("https://www.dlsite.com/maniax/download/=/product_id/%1.html").arg(id)));
			request.setRawHeader("cookie", cookies);
			request.setRawHeader("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36");
			request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
			auto reply = manager.head(request);
			connect(reply, &QNetworkReply::finished, this, std::bind(&DLSiteClient::onReceiveDownloadTry, this, reply, id,0));
		}
	}
	emit downloadStatusChanged();
}

void DLSiteClient::StartRename(const QStringList& _files)
{
	if (this->running)
		return;
	//因为都是在主线程运行，所以这里不需要用原子操作
	running = true;
	status.clear();
	local_files.clear();
	auto reg = DBProxyServer::GetWorkNameExp();
	for (const auto& file : _files)
	{
		reg.indexIn(file);
		QString id = reg.cap(0);
		if(QDir(file).dirName().size()<id.size()+2)//愿来已经有名字的不需要重命名
			this->local_files.push_back(file);
	}
	for (const auto& file : local_files)
	{
		reg.indexIn(file);
		QString id=reg.cap(0);
		status[id];
		{
			QNetworkRequest request(QUrl(QString("https://www.dlsite.com/maniax/product/info/ajax?product_id=%1&cdn_cache_min=0").arg(id)));
			//request.setRawHeader("cookie", cookies);
			request.setRawHeader("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36");
			request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
			auto reply = manager.get(request);
			connect(reply, &QNetworkReply::finished, this, std::bind(&DLSiteClient::onReceiveProductInfo, this, reply, id));
		}
	}
	//为了让没有文件需要重命名时也有响应
	emit renameStatusChanged();
}
