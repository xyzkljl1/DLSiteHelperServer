# DLSiteHelperServer
DLSiteHelper的本地Server,需要使用本地的mysql数据库
用于提供插件和数据库的交互、同步已下载商品列表、批量下载


使用前需要:
在浏览器上登录DLSite并具有合法的cookie
在本地运行mysql服务端，使用build_database.sql创建数据库  
于DBProxyServer.h/DLSiteClient.h设置本地存储和下载地址  
另行安装Qt5.14、boost1.74、VS2017(可选)  
安装并运行IDM(Internet Download Manager)  
在DBProxyServer.h中配置本地目录  
(如果不使用VS工程)配置对Qt、mysql、boost、[booost.http](https://github.com/BoostGSoC14/boost.http)、[IDMHelper](https://github.com/unamer/IDMHelper)的依赖  
确保mysql/openssl/Qt的dll对程序可见  
