#pragma once
#include <QStringList>
class DLConfig
{
public:
	//�洢Ŀ¼��λ��local_dirs���ļ�������������
	static const QStringList DLConfig::local_dirs;
	//λ��local_tmp_dirs���ļ�������������
	static const QStringList DLConfig::local_tmp_dirs;
	//����Ŀ¼,������ᱻ���ص����Ŀ¼
	static const std::string DLConfig::DOWNLOAD_DIR;
	//�����˿ڣ����DLSiteHelperһ��
	static const int SERVER_PORT = 4567;
	//���ݿ�˿�
	static const int DATABASE_PORT = 4321;
};

