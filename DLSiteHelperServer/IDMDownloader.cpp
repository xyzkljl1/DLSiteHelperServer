#include "IDMDownloader.h"
#import "IDManTypeInfo.tlb" 
#include "IDManTypeInfo.h"            
#include "IDManTypeInfo_i.c"
void SendTaskToIDM(/*StateMap status*/)
{
	ICIDMLinkTransmitter2* pIDM;
	HRESULT hr = CoCreateInstance(CLSID_CIDMLinkTransmitter,
		NULL,
		CLSCTX_LOCAL_SERVER,
		IID_ICIDMLinkTransmitter2,
		(void**)&pIDM);
	/*int ct = 0;
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

			std::string path = DLConfig::DOWNLOAD_DIR;
			switch (task.type) {
			case WorkType::AUDIO:path += "ASMR/"; break;
			case WorkType::VIDEO:path += "Video/"; break;
			case WorkType::PICTURE:path += "CG/"; break;
			case WorkType::PROGRAM:path += "Game/"; break;
			default:path += "Default/";
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
				int flags = 0x01 | 0x02;//0x01:不确认，0x02:稍后下载

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
	printf("%d in %zd Task Created,%d is not for download\n", ct, status.size() - no_download_ct, no_download_ct);
	*/
}