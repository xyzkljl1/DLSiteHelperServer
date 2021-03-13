# DLSiteHelperServer  
[DLSiteHelper](https://github.com/xyzkljl1/DLSiteHelper)的本地Server,需要使用本地的mysql数据库  
用于提供插件和数据库的交互、同步已下载商品列表、批量下载解压、批量重命名  
支持64位Windows  

Server side for [DLSiteHelper](https://github.com/xyzkljl1/DLSiteHelper).Provides database operation、batch downloading、batch renaming.  
Only support Windows x64


## Build and Run  
安装Qt5.14、VS2017、mysql server(>=8.0)  
将openssl/tufao/cpr/mysql的dll(在库里)以及Qt的dll(自行安装)加入环境路径或拷贝到运行目录  
在本地指定端口(默认4321)运行mysql服务端，使用build_database.sql创建数据库  
于本地指定端口(默认8000)运行代理  
在Chrome上登录DLSite并运行DLSiteHelper插件  
于config.json配置下载/存储目录和端口  
确保程序要使用的端口(默认4567/4319)空闲  


Install Qt5.14、VS2017、mysql server(>=8.0)  
Copy dll files in openssl/tufao/cpr/mysql directory and Qt installation dir to work directory  
Run mysql server in local port(4321 by default)  
(When neccessary)Run proxy in local port(8000 by default)  
Run DLSiteHelper in chrome And login into DLSite  
Config directory in config.json 
Make sure other local port(4567/4319 by default) is available  

  
## Reference  
  
* https://github.com/unamer/IDMHelper  
* https://github.com/sxei/chrome-plugin-demo  

## Other 

在aria2/aria2.conf里设置file-allocation=falloc时需以管理员权限运行以获得最佳性能(实际没多大区别)  

解决 浏览DLSite时部分请求过慢导致网页一直转圈圈无法触发DLSiteHelper的注入脚本的问题:   
目前不知道怎么屏蔽垃圾域名,暂定用改host和代理的方式缓解  
在host里添加(垃圾请求):  
35.227.248.159 pixel.tapad.com  
35.211.114.141 x.bidswitch.net  
35.244.245.222 idsync.rlcdn.com  
50.116.194.21 r.turn.com  
用SwitchyOmega设置代理规则：  
file.chobit.cc 走直连(下载试听)  
idsync.rlcdn.com 走专用代理(垃圾请求)  
其它请求走岛风Go  


## TODO

修复有时页面加载完，但插件没有去除已阅作品的bug  
修复购物车页面和添加至购物车的弹出窗口下方的列表未去除已阅作品的bug  