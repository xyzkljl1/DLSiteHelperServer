#include "DLConfig.h"
const QStringList DLConfig::local_dirs = {
							QString::fromLocal8Bit("G:/ASMR_archive/"),
							QString::fromLocal8Bit("G:/ASMR_cant_archive/"),
							QString::fromLocal8Bit("G:/ASMR_Chinese/"),
							QString::fromLocal8Bit("G:/DL_Pic/"),
							QString::fromLocal8Bit("G:/DL_Anime/"),
							QString::fromLocal8Bit("G:/DL_Game/"),
							QString::fromLocal8Bit("D:/IDMDownload/avater/")
							};
//ֻ����rename,������������
const QStringList DLConfig::local_tmp_dirs = {
							QString::fromLocal8Bit("D:/IDMDownload/tmp/")
							};

const std::string DLConfig::DOWNLOAD_DIR = "D:/IDMDownload/AutoDownload/";