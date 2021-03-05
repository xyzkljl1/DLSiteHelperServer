# DLSiteHelperServer  
[DLSiteHelper](https://github.com/xyzkljl1/DLSiteHelper)的本地Server,需要使用本地的mysql数据库  
用于提供插件和数据库的交互、同步已下载商品列表、批量下载、批量重命名  


使用前需要:  
在Chrome上登录DLSite运行DLSiteHelper插件  
在本地4321端口运行mysql服务端，使用build_database.sql创建数据库  
于DLConfig.h/cpp配置目录和端口  
安装Qt5.14、VS2017(可选)  
安装并运行[IDM(Internet Download Manager)](http://www.internetdownloadmanager.com)  
(如果不使用VS工程)配置对Qt、mysql、tufao、cpr、[IDMAPI](http://www.internetdownloadmanager.com/support/idm_api.html)的依赖  
确保openssl/tufao/cpr下的dll以及mysql/Qt的dll对程序可见  
确保端口4567空闲  
  
## Reference  
  
* https://github.com/unamer/IDMHelper  
* https://github.com/sxei/chrome-plugin-demo  

## Other 

(暂定)解决某些请求过慢的问题:

在host里添加:

35.227.248.159 pixel.tapad.com

35.211.114.141 x.bidswitch.net

35.244.245.222 idsync.rlcdn.com

50.116.194.21 r.turn.com

用SwitchyOmega设置代理规则：

file.chobit.cc直连(试听)

idsync.rlcdn.com走专用代理(垃圾请求)

其它请求走岛风Go


## TODO

修复有时页面加载完，但插件没有去除已阅作品的bug

修复购物车页面和添加至购物车的弹出窗口下方的列表未去除已阅作品的bug

修复下载链接在一个小时后会话过期的bug