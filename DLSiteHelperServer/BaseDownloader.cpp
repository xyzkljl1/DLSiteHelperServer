#include "BaseDownloader.h"
#include <QFile>

bool BaseDownloader::CheckFiles(const std::vector<QString>& files)
{
	for (const auto& file : files)
		if (!QFile(file).exists())
			return false;
	return true;
}
