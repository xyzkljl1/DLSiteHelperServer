#pragma once
#include "BaseDownloader.h"
class IDMDownloader:public BaseDownloader
{
public:
	IDMDownloader(){}
	bool StartDownload(const std::vector<Task>& _tasks, const cpr::Cookies& _cookie, const cpr::UserAgent& _user_agent) override { return true; }
};

