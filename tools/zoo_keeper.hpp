#ifndef ZOO_KEEPER_HPP_INCLUDE_
#define ZOO_KEEPER_HPP_INCLUDE_
#include "../system/include_sys.h"
#include "boost_log.hpp"
#include <zookeeper.h>
#include <string>
#include <boost/thread.hpp>

const int buffer_len = 4069; // zookeeper ȡֵ�ַ�����������С

/**
 * \brief zookeeper ������, ��װ��ֵ �� ȡֵ �Ȳ���
 *
 * \author LYL QQ-331461049
 * \date 2015/08/12 13:31
 */
class zoo_keeper
{
public:
	zoo_keeper(std::string hostPort);
	~zoo_keeper();

	std::string get_data(std::string node);
	bool set_data(std::string node, std::string value);

private:
	zhandle_t *zh;	        // ָ��zookeeper��Ⱥ��ָ��
	clientid_t myid;        // ���浱ǰ����zookeeper��Ⱥ�Ŀͻ���ID
	std::string m_hostPort; // zookeeper��Ⱥ�� IP:�˿� ��ʶ
};

/**
 * \brief
 *
 * \param host ���ŷָ��� IP:�˿� ��ʶ, ÿ������һ�� zk �����, ��:"127.0.0.1:3000,127.0.0.1:3001,127.0.0.1:3002"
 */
zoo_keeper::zoo_keeper(std::string hostPort)
{
	zh = zookeeper_init(hostPort.c_str(), NULL, 30000, &myid, 0, 0);
	if (!zh) {
		BOOST_LOG_SEV(cia_g_logger, Critical) << "�޷�����zookeeper������";
		throw std::exception("�޷�����zookeeper������");
	}
	while (zoo_state(zh) != ZOO_CONNECTED_STATE)
	{
		boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
	}
	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "zookeeper�Ѿ���ʼ�����";
}

zoo_keeper::~zoo_keeper()
{
	zookeeper_close(zh);
	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "zookeeper�������";
}

/**
 * \brief ͨ��zookeeper·����ַ��ȡ��Ӧ��ֵ
 *
 * \param node ·���ڵ�, ע������/��ͷ
 * \return �ڵ��Ӧ��ֵ, ����zookeeper������, ���нڵ�ֵ��Ϊ�ַ�������
 */
std::string zoo_keeper::get_data(std::string node)
{
	BOOST_LOG_SEV(cia_g_logger, Debug) << "zookeeper:��ȡֵ�ڵ�Ϊ" << node;
	if (node.at(0) != '/')
	{
		BOOST_LOG_SEV(cia_g_logger, Debug) << "zookeeper:get_data ��ηǷ�, �ڵ���Ϣ������ / ��ͷ, ����Ĳ���Ϊ" << node;
		return "";
	}
	char buffer[4069];             // �����ȡ���Ľڵ�ֵ
	int re_value_len = buffer_len; // �ڵ�ֵ�ĳ���, ע��˱���ͬʱ����������, 1:���, ����zookeeper����������, 2:����, ���ߺ��������߽ڵ�ֵ�ĳ���
	int rc;
	rc = zoo_get(zh, node.c_str(), 0, buffer, &re_value_len, NULL);
	switch (rc)
	{
	case ZOK:
		return std::string(buffer, re_value_len);
		break;
	case ZNONODE:
		BOOST_LOG_SEV(cia_g_logger, Critical) << "zookeeper:zoo_aget ��������ȡ�ڵ㲻����";
		break;
	case ZNOAUTH:
		BOOST_LOG_SEV(cia_g_logger, Critical) << "zookeeper:zoo_aget ����, ��ǰ�ͻ��˲��߱��˽ڵ�Ȩ��";
		break;
	case ZBADARGUMENTS:
		BOOST_LOG_SEV(cia_g_logger, Critical) << "zookeeper:zoo_aget ������ηǷ�";
		break;
	case ZINVALIDSTATE:
		BOOST_LOG_SEV(cia_g_logger, Critical) << "zookeeper:zhandle ��״̬Ϊ ZOO_SESSION_EXPIRED_STATE �� ZOO_AUTH_FAILED_STATE";
		break;
	case ZMARSHALLINGERROR:
		BOOST_LOG_SEV(cia_g_logger, Critical) << "zookeeper:����ʧ��, �����ڴ��Ѿ����";
		break;
	default:
		break;
	}
	return "";
}

/**
 * \brief ͨ��zookeeper·����ַ���ö�Ӧ��ֵ
 *
 * \param node ·���ڵ�, ע������/��ͷ
 * \param value �����õ�ֵ
 * \return true ���óɹ� false ����ʧ��
 */
bool zoo_keeper::set_data(std::string node, std::string value)
{
	if (node.at(0) != '/')
	{
		BOOST_LOG_SEV(cia_g_logger, Debug) << "zookeeper:get_data ��ηǷ�, �ڵ���Ϣ������ / ��ͷ, ����Ĳ���Ϊ" << node;
		return false;
	}
	int rc = zoo_set(zh, node.c_str(), value.c_str(), value.size(), -1);
	if (rc == ZOK)
	{
		return true;
	}
	else
	{
		return false;
	}
}

#endif // !ZOO_KEEPER_HPP_INCLUDE_