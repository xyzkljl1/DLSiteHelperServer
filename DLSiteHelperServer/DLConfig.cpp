#include "DLConfig.h"
const QStringList DLConfig::local_dirs = {
							QString::fromLocal8Bit("G:/ASMR_archive/"),
							QString::fromLocal8Bit("G:/ASMR_cant_archive/"),
							QString::fromLocal8Bit("G:/ASMR_Chinese/"),
							QString::fromLocal8Bit("G:/DL_Pic/"),
							QString::fromLocal8Bit("G:/DL_Anime/"),
							QString::fromLocal8Bit("G:/DL_Game/"),
							QString::fromLocal8Bit("E:/IDMDownload/avater/")
							};
//只参与rename,不视作已下载
const QStringList DLConfig::local_tmp_dirs = {
							QString::fromLocal8Bit("E:/IDMDownload/tmp/")
							};

const std::string DLConfig::DOWNLOAD_DIR = "E:/IDMDownload/AutoDownload/";