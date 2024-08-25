module;
#include <print>
export module DataBase:Impl;
//分区相当于又声明了一个模块单元，因此需要是ixx
//主单元中代码的对此文件不可见，而主模块(DataBase.ixx/DataBase.cpp)因为import了此分区可以看见此分区代码
void ImplTestFunc()
{
	printf("111");
}