#pragma once
#include <QStringList>
class DLConfig
{
public:
	//�洢Ŀ¼��λ��local_dirs���ļ�������������
	static const QStringList local_dirs;
	//λ��local_tmp_dirs���ļ�������������
	static const QStringList local_tmp_dirs;
	//����Ŀ¼,������ᱻ���ص����Ŀ¼
	static const QString DOWNLOAD_DIR;
	//�����˿ڣ����DLSiteHelperһ��
	static const int SERVER_PORT = 4567;
	//���ݿ�˿�
	static const int DATABASE_PORT = 4321;
	static const std::string REQUEST_PROXY;
	static const std::string REQUEST_PROXY_TYPE;
	//ARIA2�˿ںʹ���
	static const int ARIA2_PORT = 4319;
	static const QString ARIA2_PROXY;
	static const std::string ARIA2_SECRET;
};

