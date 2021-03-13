#include "DLSiteClient.h"
#include "DLSiteHelperServer.h"
#include <regex>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include "cpr/cpr.h"
#include "Aria2Downloader.h"
#include "IDMDownloader.h"
DLSiteClient::DLSiteClient()
{
	if (DLConfig::IDM_Mode)
		downloader = new IDMDownloader();
	else
		downloader = new Aria2Downloader();
	connect(downloader, &BaseDownloader::signalDownloadAborted, this, &DLSiteClient::OnDownloadAborted);
	connect(downloader, &BaseDownloader::signalDownloadDone, this, &DLSiteClient::OnDownloadDone);
}

DLSiteClient::~DLSiteClient()
{
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

void DLSiteClient::OnDownloadDone()
{
	if (!running)
		return;
	printf("Download Done\n");
	running = false;
}

void DLSiteClient::OnDownloadAborted()
{
	if (!running)
		return;
	printf("Download Aborted\n");
	running = false;
}

bool DLSiteClient::RenameFile(const QString& file,const QString& id,const QString&work_name)
{
	QString name = unicodeToString(work_name);
	name.replace(QRegExp("[/\\\\?*<>:\"|.]"), "_");
	int pos = file.lastIndexOf(QRegExp("[\\/]")) + 1;
	QString old_name = QDir(file).dirName();
	QString new_name = id + " " + name;
	if (old_name == new_name)
		return false;
	QDir parent(file);
	parent.cdUp();
	while (parent.exists(new_name))
		new_name = new_name + "_repeat";
	if (old_name == new_name)
		return false;
	if (parent.rename(old_name, new_name))
		return true;
	qDebug() << "Cant Rename " << old_name << "->" << new_name;
	return false;
}
void DLSiteClient::RenameThread(QStringList local_files)
{
	cpr::Session session;//����Ҫcookie
	session.SetVerifySsl(cpr::VerifySsl{ false });
	session.SetProxies(cpr::Proxies{ {std::string("https"), std::string("127.0.0.1:8000")} });
	session.SetHeader(cpr::Header{ {"user-agent","Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.88 Safari/537.36"} });
	session.SetRedirect(true);

	int ct=0;
	auto reg = DLSiteHelperServer::GetWorkNameExp();
	for (const auto& file : local_files)
	{
		reg.indexIn(file);
		QString id = reg.cap(0);
		std::string url = Format("https://www.dlsite.com/maniax/product/info/ajax?product_id=%s&cdn_cache_min=0", q2s(id).c_str());
		session.SetUrl(url);
		auto res=session.Get();
		if (res.status_code == 200)
		{
			std::smatch result;
			if (std::regex_search(res.text, result, std::regex("\"work_name\"[:\" ]*([^\"]*)\",")))
			{
				std::string file_name = result[1];
				ct += RenameFile(file, id, s2q(file_name));
			}
		}
	}
	running = false;
}
void DLSiteClient::DownloadThread(QStringList works,cpr::Cookies cookie,cpr::UserAgent user_agent)
{
	works = QStringList{ works[1] };

	std::vector<Task> task_list;
	std::map<std::string, std::future<Task>> futures;
	QStringList tmp;
	for (const auto& id : works)
		futures[q2s(id)] = std::async(&TryDownloadWork, q2s(id), cookie,user_agent,false);
	for (auto& pair : futures)//Ӧ����whenall,���ǲ�û��
		task_list.push_back(pair.second.get());
	if (!downloader->StartDownload(task_list, cookie, user_agent))
	{
		printf("Cant Start Download\n");
		running = false;
		return;
	}
	else
		printf("Fetch Done,Download Begin\n");
}
Task DLSiteClient::TryDownloadWork(std::string id,cpr::Cookies cookie, cpr::UserAgent user_agent, bool only_refresh_cookie) {
	Task task;
	task.id = id;
	cpr::Session session;
	session.SetVerifySsl(cpr::VerifySsl{ false });
	session.SetProxies(cpr::Proxies{ {DLConfig::REQUEST_PROXY_TYPE, DLConfig::REQUEST_PROXY} });
	session.SetCookies(cookie);
	session.SetUserAgent(user_agent);
	//url�ĵ�һ������������Ϊmanix��pro��books������ʵ�ʿ��������
	//��Ʒҳ��ĵڶ��������Ƿ��۷�Ϊwork��announce,���صĿ϶�����work
	//��ȡ����
	{
		session.SetRedirect(true);
		std::string url=Format("https://www.dlsite.com/maniax/work/=/product_id/%s.html",id.c_str());
		session.SetUrl(cpr::Url{ url });
		auto res=session.Get();
		if (res.status_code == 200)
			task.type = GetWorkTypeFromWeb(res.text,id);
		else
			task.type = WorkType::UNKNOWN;
	}
	if (task.type == WorkType::CANTDOWNLOAD)
		return task;
	//��ȡ��ʵ����
	session.SetRedirect(false);//����ʹ���Զ��ض�����Ϊ��Ҫ��¼�ض���ǰ��set-cookie�Լ��б��Ƿ�ֶ�����
	std::string url = Format("https://www.dlsite.com/maniax/download/=/product_id/%s.html", id.c_str());
	int idx = 0;//��ǰ������0��ʾ����/��ε�������ҳ��1~n��ʾ������صĵ�1~nҳ
	while (true) {
		session.SetUrl(cpr::Url{ url });
		auto res = session.Head();
		if (res.header.count("set-cookie"))
		{
			task.cookie = "";
			cpr::Cookies download_cookie = cookie;
			QString new_cookie = res.header["set-cookie"].c_str();
			for (auto&pair : new_cookie.split(';'))
			{
				pair = pair.trimmed();
				if (pair.size() > 1)
				{
					auto tmp = pair.split('=');
					download_cookie[q2s(tmp[0])] = q2s(tmp[1]);
				}
			}
			for (auto&pair : download_cookie)
				task.cookie += (pair.first + "=" + pair.second + "; ").c_str();
			if (only_refresh_cookie)
				return task;
		}
		if (res.status_code == 302)
		{
			if (res.header["location"] == "Array")//�ֶ����أ���ʱ��ʼ��ַ��һ��(Ϊhttps://www.dlsite.com/maniax/download/split/=/product_id/%1.html),��ͼ�õ������ص�url����ʱ���ض���"Array"
												  //û��Ҫ��ȡ������ҳ��ֱ�Ӵӵ�һ�ο�ʼ����
				url = Format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html",++idx, id.c_str());
			else//һ���ض���
				url = res.header["location"];
		}
		else if (res.status_code == 200)
		{
			QString type = res.header["Content-Type"].c_str();
			if (type.contains("application/zip") || type.contains("binary/octet-stream") 
				|| type.contains("application/octet-stream") || type.contains("application/octet-stream") 
				|| type.contains("application/x-msdownload") || type.contains("application/x-rar-compressed"))//��ȡ����ʵ��ַ
			{
				std::string ext;
				std::string file_name;
				std::regex reg("filename=\"(.*)\"");
				auto header_disposition = res.header["Content-Disposition"];
				//���ļ�ͷ�г������ļ���
				std::smatch reg_result;
				if (std::regex_search(header_disposition, reg_result, reg))
					file_name = reg_result[1];//0�����������1�ǵ�һ������
				else//����ַ����
				{
					std::regex reg("file/(.*)/");
					if (std::regex_match(url, reg_result, reg))
						file_name = reg_result[1];
				}
				int pos = file_name.find_last_of('.');
				if (pos >= 0)
					ext = file_name.substr(pos+1);

				task.file_names.push_back(file_name);
				task.urls.push_back(url);
				if (!ext.empty())
					task.download_ext.insert(ext);

				if (idx == 0)//��������
				{
					task.ready = true;
					break;
				}
				else //������أ�������һ��
					url = Format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html", ++idx, id.c_str());
			}
			else//ʧ�ܣ����type��html��ͨ������Ϊ��¼ʧ��/��������ҳʧ�ܣ�����ע��ҳ/����ҳ
				break;
		}
		else if (res.status_code == 404 && idx > 1)//������ص���ĩβ
		{
			task.ready = true;
			break;
		}
		else//ʧ��
			break;
	}
	if (!task.ready)
		qDebug() << "Receive " << id.c_str() << " Error:";
	return task;
}

WorkType DLSiteClient::GetWorkTypeFromWeb(const std::string& page,const std::string&id)
{
	/*
	<div class="work_genre">
	<a href="https://www.dlsite.com/maniax/fsr/=/work_category%5B0%5D/doujin/file_type/IJP/from/icon.work">
	<span class="icon_IJP" title="JPEG">JPEG</span></a>
	<a href="https://www.dlsite.com/maniax/fsr/=/work_category%5B0%5D/doujin/options/WPD/from/icon.work">
	<span class="icon_WPD" title="PDFͬ��">PDFͬ��</span>
	</a>
	</div>*/
	std::set<std::string> types;
	std::regex reg("class=\"icon_([A-Z]{1,10})\"");
	int cursor = 0;
	int start = -1;
	//�ҵ�����work_genre���div�������ͼ����
	while ((start = page.find("class=\"work_genre\">", cursor)) > 0)
	{
		start += std::string("class=\"work_genre\"").size();
		int end = page.find("</div>", start);
		std::string div = page.substr(start, end - start);
		std::smatch result;
		for(auto div_cursor = div.cbegin();std::regex_search(div_cursor,div.cend(),result,reg);
			div_cursor = result[0].second)
			types.insert(result[1]);
		cursor = end;
	}
	//���ȸ����ļ���ʽ�жϣ���θ�����Ʒ�����ж�
	//��Ϊ�е���Ʒ��ʽ��һ��Ĳ�һ��
	if (types.count("WBO"))//�ļ���ʽ:�����ר�ã������� ��������
		return WorkType::CANTDOWNLOAD;
	else if (types.count("EXE"))//�ļ���ʽ:���
		return WorkType::PROGRAM;
	else if (types.count("MP4"))//�ļ���ʽ:MP4
		return WorkType::VIDEO;
	else if (types.count("MP3") || types.count("WAV"))//�ļ���ʽ:MP3��WAV
		return WorkType::AUDIO;
	else if (types.count("IJP") || types.count("PNG") || types.count("IBP") || types.count("HTI"))//�ļ���ʽ:JPEG��BMP��HTML+ͼƬ
		return WorkType::PICTURE;
	else if (types.count("SOU") || types.count("MUS"))//��Ʒ����:����������(�������͵���Ʒ����������MS2)
		return WorkType::AUDIO;
	else if (types.count("RPG") || types.count("ACN") || types.count("STG") || types.count("SLN") || types.count("ADV")
		|| types.count("DNV") || types.count("PZL") || types.count("TYP") || types.count("QIZ")
		|| types.count("TBL") || types.count("ETC"))
		//��Ʒ����:RPG�������������ģ�⡢ð�ա�����С˵������(��tmR18����)�����֡����ա����桢����
		return WorkType::PROGRAM;
	else if (types.count("MOV"))//��Ʒ����:��Ƶ
		return WorkType::VIDEO;
	else if (types.count("MNG") || types.count("ICG"))//��Ʒ����:������CG
		return WorkType::PICTURE;
	else if (types.count("PVA"))//�ļ���ʽ:ר���������ͨ����WPDһ����ֲ�����ͼƬ���ͣ����ǲ�����ͬͼƬһ��򿪣���������
		return WorkType::OTHER;
	else if (types.count("WPD") || types.count("PDF") || types.count("icon_HTF"))//�ļ���ʽ:ר���������PDFͬ����PDF��HTML(flash)���޷�ȷ������
		return WorkType::OTHER;
	else if (types.count("AVI"))//�ļ���ʽ:AVI���޷�ȷ������
		return WorkType::OTHER;
	else if (types.count("ET3"))//��Ʒ����:����,�޷�ȷ������
		return WorkType::OTHER;
	printf("Cant identify type: %s",id.c_str());
	return WorkType::OTHER;
}

void DLSiteClient::StartDownload(const QByteArray& _cookies, const QByteArray& _user_agent, const QStringList& works)
{
	if (this->running)
	{
		printf("Busy. Reject Download Request\n");
		return;
	}
	//��Ϊ���������߳����У��������ﲻ��Ҫ��ԭ�Ӳ���
	running = true;
	cpr::Cookies cookies;
	for (auto&pair : _cookies.split(';'))
	{
		pair = pair.trimmed();
		if (pair.size() > 1)
		{
			auto tmp = pair.split('=');
			cookies[q2s(tmp[0])] = q2s(tmp[1]);
		}
	}
	std::thread thread(&DLSiteClient::DownloadThread, this,works,cookies, cpr::UserAgent(_user_agent.toStdString()));
	thread.detach();
}
void DLSiteClient::StartRename(const QStringList& _files)
{
	if (this->running)
		return;
	//��Ϊ���������߳����У��������ﲻ��Ҫ��ԭ�Ӳ���
	running = true;
	QStringList local_files;
	auto reg = DLSiteHelperServer::GetWorkNameExp();
	for (const auto& file : _files)
	{
		reg.indexIn(file);
		QString id = reg.cap(0);
		if(QDir(file).dirName().size()<id.size()+2)//ԭ���Ѿ������ֵĲ���Ҫ������
			local_files.push_back(file);
	}
	std::thread thread(&DLSiteClient::RenameThread,this,local_files);
	thread.detach();
}
