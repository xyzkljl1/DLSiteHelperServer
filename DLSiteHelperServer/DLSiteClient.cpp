#include "DLSiteClient.h"
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <qvariant.h>
#include <qlist.h>
Q_DECLARE_METATYPE(QList<QNetworkCookie>)
DLSiteClient::DLSiteClient()
{
	QNetworkProxy proxy(QNetworkProxy::ProxyType::HttpProxy, "127.0.0.1", 8000);
	manager.setNetworkAccessible(QNetworkAccessManager::Accessible);
	connect(&manager, &QNetworkAccessManager::finished, this, &DLSiteClient::onReceive);
	manager.setProxy(proxy);
}


DLSiteClient::~DLSiteClient()
{
}
void DLSiteClient::onReceive(QNetworkReply* res)
{
	if (res->error())
	{
		qDebug() << res->error();
		qDebug() << res->errorString();
	}
	else
	{
		auto ret = res->readAll();
		qDebug() << res->rawHeaderList();
		qDebug() << res->rawHeader("Content-Length");
		qDebug() << res->rawHeader("Content-Type");
		qDebug() <<res-> attribute(QNetworkRequest::HttpStatusCodeAttribute);
		//std::string html = QString(ret).toLocal8Bit().toStdString();
		//printf("%s\n",html.c_str());
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
	for(const auto& id:works)
	{
	//https://www.dlsite.com/maniax/download/=/product_id/RJ258916.html
	//"https://www.dlsite.com/maniax/download/=/number/1/product_id/RJ296230.html"
	//https://play.dlsite.com/api/dlsite/download?workno=RJ296230
	QNetworkRequest request(QUrl("https://www.dlsite.com/maniax/download/=/product_id/RJ296230.html"));
	request.setRawHeader("cookie", cookies);
	request.setRawHeader("user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/84.0.4147.135 Safari/537.36");

	request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
	manager.head(request);
	break;
	}
}
