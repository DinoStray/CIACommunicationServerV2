#ifndef CIA_SERVER_HPP_INCLUDE_
#define CIA_SERVER_HPP_INCLUDE_

#include "../system/include_sys.h"
#include "../tools/config_server.hpp"
#include "cia_client.hpp"
#include "../tools/boost_log.hpp"
#include "../cti/base_voice_card_control.hpp"

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
using namespace boost::asio;

class cia_server :
	public boost::enable_shared_from_this<cia_server>
{
public:
	typedef cia_server self_type;
	typedef boost::system::error_code error_code;
	typedef boost::shared_ptr<cia_server> ptr;

	cia_server(boost::shared_ptr<config_server> config_server_, boost::shared_ptr<base_voice_card_control> base_voice_card);
	~cia_server();
	void start();
protected:
	void handle_accept(cia_client::ptr client, const boost::system::error_code & err);
	void set_idol_channel_timer();
	void set_started_timer();
private:
	io_service                                 m_io_service_;
	ip::tcp::acceptor                          m_acceptor_;
	boost::thread_group                        m_io_comppletions_thread_;
	std::size_t                                m_io_comppletions_thread_number;
	std::size_t                                m_client_socket_timeout_elapsed;
	boost::shared_ptr<base_voice_card_control> m_base_voice_card;
	boost::shared_ptr<config_server>           m_config_server;// 配置服务对象
	deadline_timer                             m_set_idol_channel_timer;
	deadline_timer                             m_set_started_timer;
};

cia_server::cia_server(boost::shared_ptr<config_server> config_server_, boost::shared_ptr<base_voice_card_control> base_voice_card) :
m_io_service_(), m_acceptor_(m_io_service_, ip::tcp::endpoint(ip::tcp::v4(), config_server_->get_server_port())),
m_base_voice_card(base_voice_card),
m_set_idol_channel_timer(m_io_service_),
m_config_server(config_server_), m_set_started_timer(m_io_service_)
{
	m_io_comppletions_thread_number = m_config_server->get_iocp_thread_number();
	m_client_socket_timeout_elapsed = m_config_server->get_client_socket_timeout_elapsed();
	cia_client::ptr client = cia_client::new_(m_io_service_, m_config_server, m_base_voice_card);
	BOOST_LOG_SEV(cia_g_logger, Debug) << "服务器开始准备接收新的连接";
	m_acceptor_.async_accept(client->sock(), boost::bind(&cia_server::handle_accept, this, client, _1));
	BOOST_LOG_SEV(cia_g_logger, Debug) << "服务器开始创建异步IO处理线程";
	//TODO 暂设
	for (std::size_t i = 0; i < 24; i++)
	{
		m_io_comppletions_thread_.create_thread([this](){
			m_io_service_.run();
		});
	}
}

void cia_server::handle_accept(cia_client::ptr client, const boost::system::error_code & err)
{
	if (err)
	{
		BOOST_LOG_SEV(cia_g_logger, Debug) << "服务端已停止接收新的客户端连接";
		return;
	}
	BOOST_LOG_SEV(cia_g_logger, Debug) << "服务器接收到新的客户端连接";
	client->start();
	cia_client::ptr new_client = cia_client::new_(m_io_service_, m_config_server, m_base_voice_card);
	BOOST_LOG_SEV(cia_g_logger, Debug) << "服务器开始准备接收新的连接";
	m_acceptor_.async_accept(new_client->sock(), boost::bind(&cia_server::handle_accept, this, new_client, _1));
}

cia_server::~cia_server()
{
	m_io_service_.stop();
	BOOST_LOG_SEV(cia_g_logger, Debug) << "服务器析构";
}

void cia_server::set_idol_channel_timer()
{
	m_set_idol_channel_timer.expires_from_now(boost::posix_time::milliseconds(m_config_server->get_cti_set_idol_channel_num_elapsed()));
	BOOST_LOG_SEV(cia_g_logger, AllEvent) << "开始准备定时设置空闲通道数量";
	ptr self = shared_from_this();
	m_set_idol_channel_timer.async_wait([this, self](const error_code& ec){
		if (ec)
		{
			BOOST_LOG_SEV(cia_g_logger, Debug) << "已停止定时设置空闲通道数量";
			return;
		}
		else
		{
			m_config_server->set_idol_channel_number(m_base_voice_card->get_idol_channel_number());
			set_idol_channel_timer();
		}
	});
}

void cia_server::start()
{
	m_config_server->set_idol_channel_number(m_base_voice_card->get_idol_channel_number());
	if (m_config_server->get_cti_set_idol_channel_num_elapsed() != 0)
	{
		set_idol_channel_timer();
	}
	set_started_timer();
}

void cia_server::set_started_timer()
{
	m_set_started_timer.expires_from_now(boost::posix_time::milliseconds(5000));
	BOOST_LOG_SEV(cia_g_logger, AllEvent) << "开始准备定时设置zookeeper启动状态";
	ptr self = shared_from_this();
	m_set_started_timer.async_wait([this, self](const error_code& ec){
		if (ec)
		{
			BOOST_LOG_SEV(cia_g_logger, AllEvent) << "已停止定时设置zookeeper启动状态";
			return;
		}
		else
		{
			if (m_config_server->get_started() == 2) // 1 开启 2 关闭 -1 获取节点值失败
			{
				m_config_server->set_started(true);
			}
			set_started_timer();
		}
	});
}

#endif	// !CIA_SERVER_HPP_INCLUDE_