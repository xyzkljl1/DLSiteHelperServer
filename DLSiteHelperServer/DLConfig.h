#pragma once
#include <QStringList>
class DLConfig
{
public:
	//�洢Ŀ¼��λ��local_dirs���ļ�������������
	static QStringList local_dirs;
	//λ��local_tmp_dirs���ļ�������������
	static QStringList local_tmp_dirs;
	//����Ŀ¼,������ᱻ���ص����Ŀ¼
	static QString DOWNLOAD_DIR;
	//�����˿ڣ����DLSiteHelperһ��
	static int SERVER_PORT;
	//���ݿ�˿�
	static int DATABASE_PORT;
	static std::string REQUEST_PROXY;
	static std::string REQUEST_PROXY_TYPE;
	//ARIA2�˿ںʹ���
	static int ARIA2_PORT;
	static QString ARIA2_PROXY;
	static std::string ARIA2_SECRET;
	static bool LoadFromFile(QString file_name);
};

