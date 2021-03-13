#pragma once
#include <string>
#include <QString>
#include <set>
#include <vector>
extern std::string Format(const char * Format, ...);
extern std::string q2s(const QString&);
extern std::string q2s(const QByteArray&);
extern QString s2q(const std::string&);
//�ϳ���Ϊstd::string�õĶ���Ϊstd::string,����ΪQString
//һ����QStringһ����std::string�Һ÷���
enum WorkType {
	UNKNOWN,//��δ��ȡ��
	VIDEO,//��Ƶ
	AUDIO,//��Ƶ
	PICTURE,//ͼƬ
	PROGRAM,//����
	OTHER,//��ȡʧ��/��ȡ���˵���֪����ʲô
	CANTDOWNLOAD//�����ר�ã��޷�����
};
struct Task {
	std::string id;//RJ��
	WorkType type = UNKNOWN;//��Ʒ������
	std::set<std::string> download_ext;//���ص��ļ�(��ѹǰ)�ĸ�ʽ
	std::vector<std::string> urls;
	std::vector<std::string> file_names;//���ص��ļ����ļ���
	QString work_name;//��Ʒ��
	bool ready = false;
	std::string cookie;
	std::string GetDownloadDir();
	std::string GetDownloadPath(int i);
};
