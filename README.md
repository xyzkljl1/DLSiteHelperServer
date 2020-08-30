# DLSiteHelperServer
DLSiteHelper的本地Server,需要使用本地的mysql数据库
用于提供插件和数据库的交互、同步已下载商品列表、批量下载(未实现)


使用前需要:
安装mysql，使用build_database.sql创建数据库  
安装Qt5.14、boost1.74  
在DBProxyServer.h中配置本地目录  
(如果不使用VS工程)配置Qt、mysql、boost、booost.http的依赖  
确保mysql/openssl/Qt的dll对程序可见  
