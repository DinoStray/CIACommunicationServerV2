#ifndef CIA_CLIENT_HPP_INCLUDE_
#define CIA_CLIENT_HPP_INCLUDE_

#include "../system/include_sys.h"
#include "chat_message.hpp"
#include "CIA_DEF.h"
#include "../tools/blocking_queue.hpp"
#include "../tools/boost_log.hpp"
#include "base_client.hpp"
#include "../cti/base_voice_card_control.hpp"

#include <boost/enable_shared_from_this.hpp>
#include <boost/utility.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

const std::size_t TIMEOUT_CHECK_ELAPSED = 3000;

using namespace boost::asio;
using namespace boost::posix_time;

class cia_client;
typedef boost::shared_ptr<cia_client> client_ptr;
std::vector<client_ptr> clients;

class cia_client : 
	public boost::enable_shared_from_this<cia_client>,
	public boost::noncopyable,
	public base_client
{
public:
	typedef cia_client self_type;
	typedef boost::system::error_code error_code;
	typedef boost::shared_ptr<cia_client> ptr;

	cia_client(io_service& service, boost::shared_ptr<config_server> config_server_, boost::shared_ptr<base_voice_card_control> base_voice_card);
	~cia_client();
	static ptr new_(io_service& service, boost::shared_ptr<config_server> config_server_, boost::shared_ptr<base_voice_card_control> base_voice_card);
	void start();
	void stop();
	bool started() const { return m_started_; }
	ip::tcp::socket & sock() { return m_sock_; }
	virtual void do_write(chat_message ch_msg);
protected:
	void do_read_header();
	void do_read_body();
	void do_timeout_check();
	void do_deal_request(chat_message ch_msg);
	void do_deal_call_out_request(ciaMessage& msg);
	void do_deal_heart_request();
	void do_deal_login_request(ciaMessage& msg);
private:
	ip::tcp::socket m_sock_;
	bool m_started_;
	boost::posix_time::ptime m_update_time;
	deadline_timer m_check_timeout_timer;
	chat_message m_read_msg_;
	blocking_queue<boost::shared_ptr<chat_message>> m_write_msg_queue_;
	std::size_t m_timeout_elapsed;
	bool m_is_login;
	boost::shared_ptr<base_voice_card_control> m_base_voice_card;
	boost::shared_ptr<config_server> m_config_server;
	boost::mutex m_mutex;
};

cia_client::cia_client(io_service& service, boost::shared_ptr<config_server> config_server_, boost::shared_ptr<base_voice_card_control> base_voice_card) :
m_sock_(service), m_started_(false), m_check_timeout_timer(service), m_base_voice_card(base_voice_card),
m_config_server(config_server_)
{
	m_timeout_elapsed = m_config_server->get_client_socket_timeout_elapsed();
	std::size_t write_msg_queue_size = m_config_server->get_iocp_thread_number(); // 每个客户端socket持有的缓冲池队列, 数量暂时和iocp处理线程数量一致
	while (write_msg_queue_size--)
	{
		m_write_msg_queue_.Put(boost::make_shared<chat_message>());
	}
	m_is_login = false;
}

cia_client::ptr cia_client::new_(io_service& service, boost::shared_ptr<config_server> config_server_, boost::shared_ptr<base_voice_card_control> base_voice_card)
{
	ptr temp_new(new cia_client(service, config_server_, base_voice_card));
	return temp_new;
}

void cia_client::start()
{
	m_started_ = true;
	clients.push_back(shared_from_this());
	m_update_time = boost::posix_time::microsec_clock::local_time();
	do_read_header();
	if (m_timeout_elapsed != 0)
	{
		do_timeout_check();
	}
	BOOST_LOG_SEV(cia_g_logger, Debug) << "新的客户端socket已经运行";
}

void cia_client::stop()
{
	if (!m_started_) return;
	m_started_ = false;
	m_sock_.close();

	ptr self = shared_from_this();
	auto it = std::find(clients.begin(), clients.end(), self);
	clients.erase(it);
	BOOST_LOG_SEV(cia_g_logger, Debug) << "客户端socket已经调用stop函数关闭";
	//x m_config_server->set_started(true);	// 为了防止网络情况异常, 造成服务端关闭连接后重置此值为2, 通讯端保证此值为1
}

void cia_client::do_read_header()
{
	ptr self = shared_from_this();
	boost::asio::async_read(m_sock_,
		boost::asio::buffer(m_read_msg_.data(), chat_message::header_length),
		[this, self](boost::system::error_code ec, std::size_t /*length*/)
	{
		if (!ec && m_read_msg_.decode_header())
		{
			BOOST_LOG_SEV(cia_g_logger, Debug) << "接收新的数据, 消息体长度: " << m_read_msg_.body_length();
			do_read_body();
		}
		else
		{
			BOOST_LOG_SEV(cia_g_logger, Debug) << "接收新的数据出错, 已经关闭此socket, 错误码:  " << ec;
			stop();
		}
	});
}

void cia_client::do_read_body()
{
	ptr self = shared_from_this();
	BOOST_LOG_SEV(cia_g_logger, Debug) << "开始准备异步读取数据";
	boost::asio::async_read(m_sock_,
		boost::asio::buffer(m_read_msg_.body(), m_read_msg_.body_length()),
		[this, self](boost::system::error_code ec, std::size_t /*length*/)
	{
		if (!ec)
		{
			m_update_time = boost::posix_time::microsec_clock::local_time();
			chat_message ch_msg = m_read_msg_;
			BOOST_LOG_SEV(cia_g_logger, Debug) << "已读取新的消息体, 开始进行下一次读取";
			do_read_header();
			BOOST_LOG_SEV(cia_g_logger, Debug) << "开始解析本次请求的消息体";
			do_deal_request(ch_msg);
		}
		else
		{
			BOOST_LOG_SEV(cia_g_logger, Debug) << "接收新的数据出错, 已经关闭此socket, 错误码:  " << ec;
			stop();
		}
	});
}

void cia_client::do_write(chat_message ch_msg)
{
	boost::shared_ptr<chat_message> _ch_msg = m_write_msg_queue_.Take();
	BOOST_LOG_SEV(cia_g_logger, Debug) << "当前m_write_msg_queue_队列剩余元素:" << m_write_msg_queue_.Size();
	*_ch_msg = ch_msg;
	ptr self = shared_from_this();
	BOOST_LOG_SEV(cia_g_logger, Debug) << "开始准备异步发送数据";
	BOOST_LOG_SEV(cia_g_logger, Debug) << "异步发送的数据为::" << std::endl << ch_msg.m_procbuffer_msg.DebugString();
	m_mutex.lock();
	m_sock_.async_send(boost::asio::buffer(_ch_msg->data(), _ch_msg->length()),
		[this, self, _ch_msg](boost::system::error_code ec, std::size_t /*length*/){
		m_write_msg_queue_.Put(_ch_msg);
		if (ec){
			BOOST_LOG_SEV(cia_g_logger, Debug) << "异步发送数据回调函数，检测到异常， 关闭此客户端socket, 错误码" << ec;
			stop();
		}
		else
		{
			BOOST_LOG_SEV(cia_g_logger, Debug) << "已成功异步发送数据, 数据的transid:" << _ch_msg->m_procbuffer_msg.transid();
			m_update_time = boost::posix_time::microsec_clock::local_time();
		}
	});
	m_mutex.unlock();
}

void cia_client::do_timeout_check()
{
	if (m_started_)
	{
		m_check_timeout_timer.expires_from_now(boost::posix_time::milliseconds(TIMEOUT_CHECK_ELAPSED));
		ptr self = shared_from_this();
		BOOST_LOG_SEV(cia_g_logger, AllEvent) << "开始准备异步检测超时, 触发检测的时间是在"
			<< TIMEOUT_CHECK_ELAPSED << "毫秒后, " << "客户端socket超时时间设置为" << m_timeout_elapsed << "毫秒";
		m_check_timeout_timer.async_wait([this, self](const error_code& ec){
			if (ec)
			{
				BOOST_LOG_SEV(cia_g_logger, Debug) << "已停止超时检测";
				stop();
				return;
			}
			boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
			if ((now - m_update_time).total_milliseconds() > m_timeout_elapsed) {
				BOOST_LOG_SEV(cia_g_logger, Debug) << "客户端因超时关闭, 已经在"
					<< (now - m_update_time).total_milliseconds() << "毫秒内无任何动作";
				stop();
			}
			else if ((now - m_update_time).total_milliseconds() > m_timeout_elapsed / 2) {
				BOOST_LOG_SEV(cia_g_logger, AllEvent) << "向客户端发送心跳请求, 已经在"
					<< (now - m_update_time).total_milliseconds() << "毫秒内无任何动作";
				do_deal_heart_request();
			}
			do_timeout_check();
		});
	}
}

void cia_client::do_deal_request(chat_message ch_msg)
{
	ciaMessage msg;
	if (!msg.ParseFromArray(ch_msg.body(), ch_msg.body_length())) {
		BOOST_LOG_SEV(cia_g_logger, Debug) << "报文转换失败, 本次请求不做处理";
		return;
	}
	BOOST_LOG_SEV(cia_g_logger, Debug) << "本次请求的消息体内容为: " << std::endl << msg.DebugString();
	if (msg.type() == CIA_LOGIN_REQUEST){
		BOOST_LOG_SEV(cia_g_logger, Debug) << "本次请求判断为登陆请求";
		do_deal_login_request(msg);
	}
	else if (msg.type() == CIA_HEART_RESPONSE){
		BOOST_LOG_SEV(cia_g_logger, AllEvent) << "本次请求判断为心跳回应, 已对客户端的最后连接时间做更新";
		m_update_time = boost::posix_time::microsec_clock::local_time();
	}
	else if (msg.type() == CIA_CALL_REQUEST){
		BOOST_LOG_SEV(cia_g_logger, Debug) << "本次请求判断为呼叫请求";
		do_deal_call_out_request(msg);
	}
}

void cia_client::do_deal_call_out_request(ciaMessage& msg)
{
	ptr self = shared_from_this();
	m_base_voice_card->cti_callout(boost::make_shared<cti_call_out_param>(self, msg.transid(), msg.authcode(), msg.pn(), true));
}

void cia_client::do_deal_heart_request()
{
	ciaMessage msg;
	msg.set_type(CIA_HEART_REQUEST);
	do_write(chat_message(msg));
}

void cia_client::do_deal_login_request(ciaMessage& msg)
{
	msg.Clear();
	msg.set_type(CIA_LOGIN_RESPONSE);
	msg.set_status(CIA_LOGIN_SUCCESS);
	do_write(chat_message(msg));
	m_is_login = true;
}

cia_client::~cia_client()
{
	BOOST_LOG_SEV(cia_g_logger, Debug) << "已经对socket析构";
}

#endif // !CIA_CLIENT_HPP_INCLUDE_