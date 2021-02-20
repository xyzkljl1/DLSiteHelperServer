#include "DLSiteClient.h"
#include "cpr/cpr.h"
#include <QDir>
#include <QMetaType>   
#include <regex>
#import "IDManTypeInfo.tlb" 
#include "IDManTypeInfo.h"            
#include "IDManTypeInfo_i.c"
#include <comutil.h>
#include <atlthunk.h>
#include "DBProxyServer.h"
const std::string DLSiteClient::DOWNLOAD_DIR = "D:/IDMDownload/AutoDownload/";
Q_DECLARE_METATYPE(DLSiteClient::StateMap);//ɳ����ϲ��������ţ����Ա����������
DLSiteClient::DLSiteClient()
{
	qRegisterMetaType<DLSiteClient::StateMap>();
	connect(this, &DLSiteClient::signalStartDownload, this, &DLSiteClient::SendTaskToIDM);
}

DLSiteClient::~DLSiteClient()
{
}

void DLSiteClient::SendTaskToIDM(StateMap status)
{
	ICIDMLinkTransmitter2* pIDM;
	HRESULT hr = CoCreateInstance(CLSID_CIDMLinkTransmitter,
		NULL,
		CLSCTX_LOCAL_SERVER,
		IID_ICIDMLinkTransmitter2,
		(void**)&pIDM);
	int ct = 0;
	int no_download_ct = 0;
	if (hr == S_OK)
		for (const auto& task_pair : status)
		{
			const auto&task = task_pair.second;
			bool success = true;
			if (task.type == WorkType::CANTDOWNLOAD)
			{
				no_download_ct++;
				continue;
			}
			if (!task.ready)
				continue;
			if (task.urls.empty())
				continue;

			std::string path = DOWNLOAD_DIR;
			switch (task.type) {
			case WorkType::AUDIO:path += "ASMR/"; break;
			case WorkType::VIDEO:path += "Video/"; break;
			case WorkType::PICTURE:path += "CG/"; break;
			case WorkType::PROGRAM:path += "Game/"; break;
			default:path += "Default/";
			}
			//zip�Ľ�ѹ�껹��Ҫת�룬����Ҫ��rar���ֿ�
			if (task.download_ext.count("zip"))
				path += "zip/";
			else if (task.download_ext.count("rar"))
				path += "rar/";
			else
				path += "other/";
			if (!QDir(QString::fromLocal8Bit(path.c_str())).exists())
				QDir().mkpath(QString::fromLocal8Bit(path.c_str()));
			for (int i = 0; i < task.urls.size(); ++i)
			{
				VARIANT var;
				VariantInit(&var);
				var.vt = VT_EMPTY;
				_bstr_t url(task.urls[i].c_str());
				_bstr_t fname(task.file_names[i].c_str());
				_bstr_t referer("https://play.dlsite.com/#/library");
				_bstr_t path_b(path.c_str());
				_bstr_t cookies(task.cookie.toStdString().c_str());
				_bstr_t data("");
				_bstr_t user("");
				_bstr_t pass("");
				int flags = 0x01 | 0x02;//0x01:��ȷ�ϣ�0x02:�Ժ�����

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
		printf("CoCreate IDM Instacne Failed\n");
	printf("%d in %zd Task Created,%d is not for download\n",ct,status.size()- no_download_ct, no_download_ct);
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
void DLSiteClient::RenameProcess(QStringList local_files)
{
	cpr::Session session;//����Ҫcookie
	session.SetVerifySsl(cpr::VerifySsl{ false });
	session.SetProxies(cpr::Proxies{ {std::string("https"), std::string("127.0.0.1:8000")} });
	session.SetHeader(cpr::Header{ {"user-agent","Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.88 Safari/537.36"} });
	session.SetRedirect(true);

	int ct=0;
	auto reg = DBProxyServer::GetWorkNameExp();
	for (const auto& file : local_files)
	{
		reg.indexIn(file);
		QString id = reg.cap(0);
		std::string url = format("https://www.dlsite.com/maniax/product/info/ajax?product_id=%s&cdn_cache_min=0", id.toStdString().c_str());
		session.SetUrl(url);
		auto res=session.Get();
		if (res.status_code == 200)
		{
			std::smatch result;
			if (std::regex_search(res.text, result, std::regex("\"work_name\"[:\" ]*([^\"]*)\",")))
			{
				std::string file_name = result[1];
				ct += RenameFile(file, id, QString::fromLocal8Bit(file_name.c_str()));
			}
		}
	}
	running = false;
}
void DLSiteClient::DownloadProcess(cpr::Cookies cookies,cpr::UserAgent user_agent, QStringList works)
{
	StateMap status;
	std::map<std::string, std::future<State>> futures;
	for (const auto& id : works)
		futures[id.toStdString()] = std::async(TryDownloadWork, id.toStdString(), cookies,user_agent);
	for (auto& pair : futures)//Ӧ����whenall,���ǲ�û��
		status[pair.first.c_str()] = pair.second.get();
	emit signalStartDownload(status);//SendTasktoIDM���������߳����У�����Ҫͨ���źŵ���
	running = false;
}
DLSiteClient::State DLSiteClient::TryDownloadWork(std::string id, cpr::Cookies cookie, cpr::UserAgent user_agent) {
	DLSiteClient::State status;
	cpr::Session session;
	session.SetVerifySsl(cpr::VerifySsl{ false });
	session.SetProxies(cpr::Proxies{ {std::string("https"), std::string("127.0.0.1:8000")} });
	session.SetCookies(cookie);
	session.SetUserAgent(user_agent);
	//url�ĵ�һ������������Ϊmanix��pro��books������ʵ�ʿ��������
	//��Ʒҳ��ĵڶ��������Ƿ��۷�Ϊwork��announce,���صĿ϶�����work
	//��ȡ����
	{
		session.SetRedirect(true);
		std::string url=format("https://www.dlsite.com/maniax/work/=/product_id/%s.html",id.c_str());
		session.SetUrl(cpr::Url{ url });
		auto res=session.Get();
		if (res.status_code == 200)
			status.type = FindWorkTypeFromWeb(res.text,id);
		else
			status.type = WorkType::UNKNOWN;
	}
	if (status.type == WorkType::CANTDOWNLOAD)
		return status;
	//��ȡ��ʵ����
	session.SetRedirect(false);//����ʹ���Զ��ض�����Ϊ��Ҫ��¼�ض���ǰ��set-cookie�Լ��б��Ƿ�ֶ�����
	std::string url = DLSiteClient::format("https://www.dlsite.com/maniax/download/=/product_id/%s.html", id.c_str());
	int idx = 0;//��ǰ������0��ʾ����/��ε�������ҳ��1~n��ʾ������صĵ�1~nҳ
	while (true) {
		session.SetUrl(cpr::Url{ url });
		auto res = session.Head();
		if (res.header.count("set-cookie"))//���ص�cookie����������
			status.cookie = res.header["set-cookie"].c_str();
		if (res.status_code == 302)
		{
			if (res.header["location"] == "Array")//�ֶ����أ���ʱ��ʼ��ַ��һ��(Ϊhttps://www.dlsite.com/maniax/download/split/=/product_id/%1.html),��ͼ�õ������ص�url����ʱ���ض���"Array"
												  //û��Ҫ��ȡ������ҳ��ֱ�Ӵӵ�һ�ο�ʼ����
				url = DLSiteClient::format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html",++idx, id.c_str());
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

				status.file_names.push_back(file_name);
				status.urls.push_back(url);
				if (!ext.empty())
					status.download_ext.insert(ext);

				if (idx == 0)//��������
				{
					status.ready = true;
					break;
				}
				else //������أ�������һ��
					url = DLSiteClient::format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html", ++idx, id.c_str());
			}
			else//ʧ�ܣ����type��html��ͨ������Ϊ��¼ʧ��/��������ҳʧ�ܣ�����ע��ҳ/����ҳ
				break;
		}
		else if (res.status_code == 404 && idx > 1)//������ص���ĩβ
		{
			status.ready = true;
			break;
		}
		else//ʧ��
			break;
	}
	if (!status.ready)
		qDebug() << "Receive " << id.c_str() << " Error:";
	return status;
}

DLSiteClient::WorkType DLSiteClient::FindWorkTypeFromWeb(const std::string& page,const std::string&id)
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

void DLSiteClient::StartDownload(const QByteArray& _cookies, const QByteArray& user_agent, const QStringList& works)
{
	if (this->running)
		return;
	//��Ϊ���������߳����У��������ﲻ��Ҫ��ԭ�Ӳ���
	running = true;
	cpr::Cookies cookies;
	for (auto&pair : _cookies.split(';'))
	{
		pair = pair.trimmed();
		if (pair.size() > 1)
		{
			auto tmp = pair.split('=');
			cookies[tmp[0].toStdString()] = tmp[1];
		}
	}

	//DownloadProcess(pIDM, cookies, works);
	std::thread thread(&DLSiteClient::DownloadProcess, this, cookies,cpr::UserAgent(user_agent.toStdString()),works);
	thread.detach();
}

std::string DLSiteClient::format(const char* format, ...)
{
	char buff[1024] = { 0 };

	va_list args;
	va_start(args, format);
	vsprintf_s(buff, sizeof(buff), format, args);
	va_end(args);

	std::string str(buff);
	return str;
}
void DLSiteClient::StartRename(const QStringList& _files)
{
	if (this->running)
		return;
	//��Ϊ���������߳����У��������ﲻ��Ҫ��ԭ�Ӳ���
	running = true;
	QStringList local_files;
	auto reg = DBProxyServer::GetWorkNameExp();
	for (const auto& file : _files)
	{
		reg.indexIn(file);
		QString id = reg.cap(0);
		if(QDir(file).dirName().size()<id.size()+2)//ԭ���Ѿ������ֵĲ���Ҫ������
			local_files.push_back(file);
	}
	std::thread thread(&DLSiteClient::RenameProcess,this,local_files);
	thread.detach();
}
