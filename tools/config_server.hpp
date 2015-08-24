#ifndef CONFIG_SERVER_HPP_INCLUDE_
#define CONFIG_SERVER_HPP_INCLUDE_

//#include "../system/include_sys.h"
#include "boost_log.hpp"
#include "cia_zookeeper.h"
#include <string>

const std::string ZOO_PATH_BASE_PATH("/cs"); // 所有通讯端统一zookeeper路径, 内含通用属性值

const std::string ZOO_PATH_FREE_CHANNEL_NUM("/freeNum");
const std::string ZOO_PATH_STATUS("/status");
const std::string ZOO_PATH_AREA_CODE("/areaCode");
const std::string ZOO_PATH_CODE_POOL("/codePool");

const std::string ZOO_PATH_IOCP_THREAD_NUM("/iocpThreadNumber");
const std::string ZOO_PATH_SERVER_PORT("/serverPort");
const std::string ZOO_PATH_CLIENT_SOCKET_TIMEOUT_ELAPSED("/clientSocketTimeoutElapsed");
const std::string ZOO_PATH_TOTAL_CHANNEL_NUMBER("/channelTotalNum");
const std::string ZOO_PATH_CIT_TIMEOUT_ELAPSED("/CTITimeOutElapsed");
const std::string ZOO_PATH_CIT_SET_IDOL_CHANNEL_NUM_ELAPSED("/CTIIdolChannelNumSetElapsed");
const std::string ZOO_PATH_CTI_WARNING_ELAPSED("/CTIWarningElapsed");
const std::string ZOO_PATH_CTI_SLEEPING_ELAPSED("/CTISleepingElapsed");
const std::string ZOO_PATH_CTI_SET_IDOL_CHANNEL_NUM_ELAPSED("/CTISetIdolChannelElapsed");

class config_server
{
public:
	config_server(std::string current_node_path, std::string zookeeper_server_ip_port);
	~config_server(){};

	void set_idol_channel_number(std::size_t idol_channel_number);
	void set_started(bool status);
	int get_started();

	std::size_t get_iocp_thread_number();
	bool flush_iocp_thread_number();
	std::size_t get_server_port();
	bool flush_server_port();
	int get_client_socket_timeout_elapsed();
	bool flush_client_socket_timeout_elapsed();
	std::size_t get_cti_total_channel_count();
	bool flush_cti_total_channel_count();
	int get_cti_set_idol_channel_num_elapsed();
	bool flush_cti_set_idol_channel_num_elapsed();
	int get_cti_timeout_elapsed();
	bool flush_cti_timeout_elapsed();
	int get_cti_warning_elapsed();
	bool flush_cti_warning_elapsed();
	int get_cti_sleeping_elapsed();
	bool flush_cti_sleeping_elapsed();
protected:
private:
	//config_server运行所需配置文件, 包含zookeeper配置文件, 以及以后可能添加的本地配置文件等
	std::string m_current_node_path;                // 当前节点的zookeeper基路径, 一般为/auth/ip
	std::string m_zookeeper_server_ip_port;         // zookeeper服务端的ip 和端口, 一般的格式为"127.0.0.1:3000,127.0.0.1:3001,127.0.0.1:3002"

	//zookeeper相关文件
	std::size_t m_idol_channel_number;              // /auth/ip/freeNum(空闲通道数量), 暂定每隔1秒更新一次
	std::string m_started;                          // /auth/ip/status(状态(1:正常 2:关闭)), 应用每次启动关闭均需设置
	std::string areaCode;                           // /auth/ip/areaCode(电话区号), 由zookeeper管理员配置, 通讯端暂不使用
	std::string codePool;                           // /auth/ip/codePool(号码池(86031111|86032222)), 由zookeeper管理员配置, 通讯端暂不使用

	//通讯端内部配置文件
	std::size_t m_iocp_thread_number;               // io 完成端口处理线程数量
	std::string m_log_config_file;	                // 日志配置文件, 由于每台服务器此属性值不一样, 故变量取值在/auth/ip/logConfigFile

	//通讯端 服务组件
	std::size_t m_server_port;	                    // 通讯端服务端口, 由于每台服务器此属性值可能不一样, 故变量取值在/auth/ip/serverPort
	std::size_t m_client_socket_timeout_elapsed;    // 客户端超时, 单位:秒

	//通讯端 语音卡组件
	std::size_t m_cti_total_channel_count;          // 语音卡中继线总数, 由于每台服务器此属性值不一样, 故变量取值在/auth/ip/channelTotalNum
	std::size_t m_cti_timeout_elapse;               // 呼叫超时强制挂断的时间, 单位毫秒
	std::size_t m_cti_warning_elapse;	            // 触发策略的呼叫响应间隔时间。 发起呼叫后， 如果在设定的时间内触发响铃， 则属于“超前”响铃时间， 需要延迟挂机
	std::size_t m_cti_sleeping_elapse;              // 触发策略后， 延迟多长时间挂机
	std::size_t m_cit_set_idol_channel_elpesed;     // 设置空闲通道数量的时间间隔, 单位毫秒
};

/**
 * \brief 维护配置信息的统一调用设置, 隔离项目属性值来源不同对程序的影响(如来自命令行, 配置文件, zookeeper)
 *        创建了此类, 对外提供统一的访问接口
 *
 * \param current_node_path 一般为/auth/ip, ip为当前通讯端服务器的IP地址, 如/auth/116.255.130.148
 * \param zookeeper_server_ip_port 逗号分隔的 IP:端口 标识, 每个关联一个 zookeeper 服务端, 如:"127.0.0.1:3000,127.0.0.1:3001,127.0.0.1:3002"
 */
config_server::config_server(std::string current_node_path, std::string zookeeper_server_ip_port) :
m_current_node_path(current_node_path), m_zookeeper_server_ip_port(zookeeper_server_ip_port)
{
	if (init_zookeeper(zookeeper_server_ip_port) == -1)
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "初始化zookeeper失败";
	}
	flush_iocp_thread_number();
	flush_server_port();
	flush_client_socket_timeout_elapsed();
	flush_cti_total_channel_count();
	flush_cti_set_idol_channel_num_elapsed();
	flush_cti_timeout_elapsed();
	flush_cti_warning_elapsed();
	flush_cti_sleeping_elapsed();
}

/**
 * \brief 设置空闲通道数, 此值会被服务端获取, 依据此值控制并发压力
 *
 * \param idol_channel_number 空闲通道数
 */
void config_server::set_idol_channel_number(std::size_t idol_channel_number)
{
	if (!zk_set_data(m_current_node_path + ZOO_PATH_FREE_CHANNEL_NUM, std::to_string(idol_channel_number)))
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "设置空闲通道数失败";
	}
}

/**
 * \brief 设置当前通讯端状态值, 应用每次启动关闭均需设置
 *
 * \param status true 通讯端开启 false 通讯端关闭
 */
void config_server::set_started(bool status)
{
	/// auth / ip / status(状态(1:正常 2 : 关闭)), 应用每次启动关闭均需设置
	if (status == true)
	{
		m_started = "1";
	}
	else
	{
		m_started = "2";
	}
	if (!zk_set_data(m_current_node_path + ZOO_PATH_STATUS, m_started))
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "设置通讯端状态失败";
	}
}

/**
 * \brief 获取 io 完成端口处理线程数量
 *
 * \return 0 获取失败 大于0值则获取成功
 */
std::size_t config_server::get_iocp_thread_number()
{
	return m_iocp_thread_number;
}

/**
 * \brief 获取通讯端端口号
 *
 * \param
 * \return 0 获取失败 大于0值则获取成功
 */
std::size_t config_server::get_server_port()
{
	return m_server_port;
}

/**
 * \brief 获取客户端socket超时时间, 在此时间段内没有任何响应, 则关闭socket
 *
 * \return 0 关闭socket超时检测, 大于0值则开启socket 超时检测, -1取值失败
 */
int config_server::get_client_socket_timeout_elapsed()
{
	return m_client_socket_timeout_elapsed;
}

/**
 * \brief 获取当前通讯端总共的语音通道数量
 *
 * \return 0, 获取失败, 大于0值则获取成功
 */
std::size_t config_server::get_cti_total_channel_count()
{
	return m_cti_total_channel_count;
}

/**
 * \brief 获取呼叫超时强制挂断的时间, 单位毫秒
 *
 * \return -1取值失败
 */
int config_server::get_cti_timeout_elapsed()
{
	return m_cti_timeout_elapse;
}

/**
 * \brief 获取触发策略的呼叫响应间隔时间。 发起呼叫后， 如果在设定的时间内触发响铃， 则属于“超前”响铃时间， 需要延迟挂机
 *
 * \return -1取值失败
 */
int config_server::get_cti_warning_elapsed()
{
	return m_cti_warning_elapse;
}

/**
* \brief 触发策略后， 延迟多长时间挂机
*
* \return -1取值失败
*/
int config_server::get_cti_sleeping_elapsed()
{
	return m_cti_sleeping_elapse;
}

bool config_server::flush_iocp_thread_number()
{
	std::string thread_num = zk_get_data(ZOO_PATH_BASE_PATH + ZOO_PATH_IOCP_THREAD_NUM);
	if (thread_num.empty())
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "获取io 完成端口处理线程数量失败";
		m_iocp_thread_number = 0;
		return false;
	}
	else
	{
		m_iocp_thread_number = std::stoi(thread_num);
		return true;
	}
}

bool config_server::flush_server_port()
{
	std::string port = zk_get_data(ZOO_PATH_BASE_PATH + ZOO_PATH_SERVER_PORT);
	if (port.empty())
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "获取通讯端端口号失败";
		m_server_port = 0;
		return false;
	}
	else
	{
		m_server_port = std::stoi(port);
		return true;
	}
}

bool config_server::flush_client_socket_timeout_elapsed()
{
	std::string timeout = zk_get_data(ZOO_PATH_BASE_PATH + ZOO_PATH_CLIENT_SOCKET_TIMEOUT_ELAPSED);
	if (timeout.empty())
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "获取客户端socket超时时间失败";
		m_client_socket_timeout_elapsed = 0;
		return false;
	}
	else
	{
		m_client_socket_timeout_elapsed = std::stoi(timeout);
		return false;
	}
}

bool config_server::flush_cti_total_channel_count()
{
	std::string total_number = zk_get_data(m_current_node_path + ZOO_PATH_TOTAL_CHANNEL_NUMBER);
	if (total_number.empty())
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "获取语音卡通道数量失败";
		m_cti_total_channel_count = 0;
		return false;
	}
	else
	{
		m_cti_total_channel_count = std::stoi(total_number);
		return true;
	}
}

bool config_server::flush_cti_timeout_elapsed()
{
	std::string time_out = zk_get_data(ZOO_PATH_BASE_PATH + ZOO_PATH_CIT_TIMEOUT_ELAPSED);
	if (time_out.empty())
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "获取通讯端端口号失败";
		m_cti_timeout_elapse = 0;
		return false;
	}
	else
	{
		m_cti_timeout_elapse = std::stoi(time_out);
		return true;
	}
}

bool config_server::flush_cti_warning_elapsed()
{
	std::string warning_elapse = zk_get_data(m_current_node_path + ZOO_PATH_CTI_WARNING_ELAPSED);
	if (warning_elapse.empty())
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "获取通讯端端口号失败";
		m_cti_warning_elapse = 0;
		return false;
	}
	else
	{
		m_cti_warning_elapse = std::stoi(warning_elapse);
		return true;
	}
}

bool config_server::flush_cti_sleeping_elapsed()
{
	std::string sleeping_elapsed = zk_get_data(m_current_node_path + ZOO_PATH_CTI_SLEEPING_ELAPSED);
	if (sleeping_elapsed.empty())
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "获取通讯端端口号失败";
		m_cti_sleeping_elapse = 0;
		return false;
	}
	else
	{
		m_cti_sleeping_elapse = std::stoi(sleeping_elapsed);
		return true;
	}
}

int config_server::get_cti_set_idol_channel_num_elapsed()
{
	return m_cit_set_idol_channel_elpesed;
}

bool config_server::flush_cti_set_idol_channel_num_elapsed()
{	
	std::string elapsed = zk_get_data(ZOO_PATH_BASE_PATH + ZOO_PATH_CTI_SET_IDOL_CHANNEL_NUM_ELAPSED);
	if (elapsed.empty())
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "获取通讯端端口号失败";
		m_cit_set_idol_channel_elpesed = 0;
		return false;
	}
	else
	{
		m_cit_set_idol_channel_elpesed = std::stoi(elapsed);
		return true;
	}
}

int config_server::get_started()
{
	std::string temp_val = zk_get_data(m_current_node_path + ZOO_PATH_STATUS);
	if (temp_val.empty())
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "获取通讯端状态失败";
		return -1;
	}
	else
	{
		return std::stoi(temp_val);;
	}
}

#endif // !CONFIG_SERVER_HPP_INCLUDE_