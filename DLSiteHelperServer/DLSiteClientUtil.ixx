module;
#include <QList>
#include "cpr/cpr.h"
export module DLSiteClientUtil;
import Util;
import DLConfig;
import <regex>;
import <set>;
using namespace Util;
namespace DLSiteClientUtil {
	export WorkType GetWorkTypeFromWeb(const std::string& page, const std::string& id)
	{
		/*
		<div class="work_genre">
		<a href="https://www.dlsite.com/maniax/fsr/=/work_category%5B0%5D/doujin/file_type/IJP/from/icon.work">
		<span class="icon_IJP" title="JPEG">JPEG</span></a>
		<a href="https://www.dlsite.com/maniax/fsr/=/work_category%5B0%5D/doujin/options/WPD/from/icon.work">
		<span class="icon_WPD" title="PDF同捆">PDF同捆</span>
		</a>
		</div>*/
		std::set<std::string> types;
		std::regex reg("class=\"icon_([A-Z0-9]{1,10})\"");
		int cursor = 0;
		int start = -1;
		//找到所有work_genre类的div里的所有图标类,即"作品形式"、"ファイル形式"、"年齢指定"、"その他"
		//有的作品仅有作品形式如VJ015443
		while ((start = page.find("class=\"work_genre\"", cursor)) > 0)
		{
			start += std::string("class=\"work_genre\"").size();
			int end = page.find("</div>", start);
			std::string div = page.substr(start, end - start);
			std::smatch result;
			for (auto div_cursor = div.cbegin(); std::regex_search(div_cursor, div.cend(), result, reg);
				div_cursor = result[0].second)
				types.insert(result[1]);
			cursor = end;
		}
		//注意顺序，根据その他、文件形式、作品形式判断
		//因为有的作品格式跟一般的不一致
		if (types.count("OTM"))//その他:乙女向け ※最优先
			return WorkType::SHIT;
		else if (types.count("WBO"))//文件形式:浏览器专用，不下载
			return WorkType::CANTDOWNLOAD;
		else if (types.count("EXE"))//文件形式:软件
			return WorkType::PROGRAM;
		else if (types.count("MP4") || types.count("MWM"))//文件形式:MP4、WMV
			return WorkType::VIDEO;
		else if (types.count("ADO"))//文件形式:オーディオ(audio)
			return WorkType::AUDIO;
		else if (types.count("MP3") || types.count("WAV") || types.count("FLC") || types.count("WMA") || types.count("AAC"))//文件形式:MP3、WAV、FLAC、WMA、AAC
			return WorkType::AUDIO;
		else if (types.count("IJP") || types.count("PNG") || types.count("IBP") || types.count("HTI"))//文件形式:JPEG、BMP、HTML+图片
			return WorkType::PICTURE;
		else if (types.count("SOU") || types.count("MUS"))//作品类型:音声sound、音乐music(其它类型的作品含有音乐是MS2)
			return WorkType::AUDIO;
		else if (types.count("RPG") || types.count("ACN") || types.count("STG") || types.count("SLN") || types.count("ADV")
			|| types.count("DNV") || types.count("PZL") || types.count("TYP") || types.count("QIZ")
			|| types.count("TBL") || types.count("ETC"))
			//作品类型:RPG、动作、射击、模拟、冒险、电子小说、益智(神tmR18益智)、打字、解谜、桌面、其它
			return WorkType::PROGRAM;
		else if (types.count("MOV"))//作品类型:视频
			return WorkType::VIDEO;
		else if (types.count("MNG") || types.count("ICG"))//作品类型:漫画、CG
			return WorkType::PICTURE;
		else if (types.count("PVA"))//文件形式:专用浏览器，通常和WPD一起出现并且是图片类型，但是不能如同图片一般打开，视作其它
			return WorkType::OTHER;
		else if (types.count("WPD") || types.count("PDF") || types.count("HTF"))//文件形式:专用浏览器、PDF同捆、PDF、HTML(flash)，无法确定类型
			return WorkType::OTHER;
		else if (types.count("AVI"))//文件形式:AVI，无法确定类型
			return WorkType::OTHER;
		else if (types.count("ET3"))//作品类型:其它,无法确定类型
			return WorkType::OTHER;
		LogError("Cant identify type: %s", id.c_str());
		return WorkType::OTHER;
	}

	export Task MakeDownloadTask(std::string id, cpr::Cookies cookie, cpr::UserAgent user_agent) {
		Task task;
		task.id = id;
		cpr::Session session;
		session.SetVerifySsl(cpr::VerifySsl{ false });
		session.SetProxies(cpr::Proxies{ {DLConfig::REQUEST_PROXY_TYPE, DLConfig::REQUEST_PROXY} });
		session.SetCookies(cookie);
		session.SetUserAgent(user_agent);
		//url的第一级根据卖场分为manix、pro、books，但是实际可以随便用
		//产品页面的第二级根据是否发售分为work、announce,下载的肯定都是work
		//获取类型
		{
			session.SetRedirect(cpr::Redirect{ true });
			std::string url = Format("https://www.dlsite.com/maniax/work/=/product_id/%s.html", id.c_str());
			session.SetUrl(cpr::Url{ url });
			auto res = session.Get();
			if (res.status_code == 200)
				task.type = GetWorkTypeFromWeb(res.text, id);
			else
				task.type = WorkType::UNKNOWN;
		}
		if (task.type == WorkType::CANTDOWNLOAD)
			return task;
		//获取真实连接
		session.SetRedirect(cpr::Redirect{ false });//不能使用自动重定向，因为需要记录重定向前的set-cookie以及判别是否分段下载
		//2023-12：maniax/download/=/product_id/%s.html不再能对多文件重定向，多文件需要直接访问maniax/download/split/=/product_id/%s.html
		//2023-12：分段下载时变为每个文件必须单独使用一个cookie,cookie决定下载哪个文件，例如使用part1的url+part2的cookie时下载的是第二个文件
		std::string url = Format("https://www.dlsite.com/maniax/download/=/product_id/%s.html", id.c_str());
		std::string current_cookie;
		int idx = 0;//当前段数，0表示单段/多段的总下载页，1~n表示多段下载的第1~n页
		while (true) {
			session.SetUrl(cpr::Url{ url });
			auto res = session.Head();
			if (res.header.count("set-cookie"))
			{
				current_cookie = "";
				cpr::Cookies download_cookie = cookie;
				QString new_cookie = res.header["set-cookie"].c_str();
				for (auto& pair : new_cookie.split(';'))
				{
					pair = pair.trimmed();
					if (pair.size() > 1)
					{
						auto tmp = pair.split('=');
						download_cookie.push_back(cpr::Cookie(q2s(tmp[0]), q2s(tmp[1])));
					}
				}
				for (auto& cookie : download_cookie)
					current_cookie += (cookie.GetName() + "=" + cookie.GetValue() + "; ").c_str();
			}
			if (res.status_code == 302)
			{
				//if (res.header["location"] == "Array")//分段下载，此时初始网址不一样(为https://www.dlsite.com/maniax/download/split/=/product_id/%1.html),试图用单段下载的url访问时会重定向到"Array"
													  //没必要获取总下载页，直接从第一段开始下载
				//	url = Format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html",++idx, id.c_str());
				//else
				url = res.header["location"];//一般重定向
			}
			else if (res.status_code == 500) {
				//如果正在访问初始下载页面且获得500，说明是分段下载，此时初始网址不一样(为https://www.dlsite.com/maniax/download/split/=/product_id/%1.html)
				//没必要获取总下载页，直接从第一段开始下载
				if (url.find("https://www.dlsite.com/maniax/download/=/product_id/") != std::string::npos)
					url = Format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html", ++idx, id.c_str());
				else
					break;
			}
			else if (res.status_code == 200)
			{
				QString type = res.header["Content-Type"].c_str();
				if (type.contains("application/zip") || type.contains("binary/octet-stream")
					|| type.contains("application/octet-stream") || type.contains("application/octet-stream")
					|| type.contains("application/x-msdownload") || type.contains("application/x-rar-compressed"))//获取到真实地址
				{
					std::string ext;
					std::string file_name;
					std::regex reg("filename=\"(.*)\"");
					auto header_disposition = res.header["Content-Disposition"];
					//从文件头中尝试找文件名
					std::smatch reg_result;
					if (std::regex_search(header_disposition, reg_result, reg))
						file_name = reg_result[1];//0是完整结果，1是第一个括号
					else//从网址中找
					{
						std::regex reg("file/(.*)/");
						if (std::regex_match(url, reg_result, reg))
							file_name = reg_result[1];
					}
					int pos = file_name.find_last_of('.');
					if (pos >= 0)
						ext = file_name.substr(pos + 1);

					task.file_names.push_back(file_name);
					task.urls.push_back(url);
					task.cookies.push_back(current_cookie);
					if (!ext.empty())
						task.download_ext.insert(ext);

					if (idx == 0)//单段下载
					{
						task.ready = true;
						break;
					}
					else //多段下载，尝试下一段
						url = Format("https://www.dlsite.com/maniax/download/=/number/%d/product_id/%s.html", ++idx, id.c_str());
				}
				else//失败，如果type是html，通常是因为登录失败/访问下载页失败，返回注册页/错误页
					break;
			}
			else if (res.status_code == 404 && idx > 1)//多段下载到达末尾
			{
				task.ready = true;
				break;
			}
			else//失败
				break;
		}
		//有的作品会被删除，例:RJ320458
		if (!task.ready)
			LogError("Receive %s Error",id.c_str());
		return task;
	}
}