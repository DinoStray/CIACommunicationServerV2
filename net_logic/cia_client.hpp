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
#include <boost/timer/timer.hpp>



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
	virtual void do_write(boost::shared_ptr<chat_message> ch_msg);
protected:
	void do_read_header();
	void do_read_body(boost::shared_ptr<chat_message> ch_msg_);
	static void do_deal_log();
	void do_timeout_check();
	void do_deal_request(boost::shared_ptr<chat_message> ch_msg);
	void do_deal_call_out_request(boost::shared_ptr<chat_message> msg);
	void do_deal_heart_request();
	void do_deal_login_request(boost::shared_ptr<chat_message> msg);
private:
	ip::tcp::socket                                 m_sock_;
	bool                                            m_started_;
	boost::timer::cpu_timer                         m_update_time;
	deadline_timer                                  m_check_timeout_timer;
	std::size_t                                     m_timeout_elapsed;
	bool                                            m_is_login;
	boost::shared_ptr<base_voice_card_control>      m_base_voice_card;
	boost::shared_ptr<config_server>                m_config_server;
	boost::thread_group                             m_deal_ch_msg_group;
};

cia_client::cia_client(io_service& service, boost::shared_ptr<config_server> config_server_, boost::shared_ptr<base_voice_card_control> base_voice_card) :
m_sock_(service), m_started_(false), m_check_timeout_timer(service), m_base_voice_card(base_voice_card),
m_config_server(config_server_)
{
	m_timeout_elapsed = m_config_server->get_client_socket_timeout_elapsed();
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
	m_update_time.start();
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
	m_deal_ch_msg_group.interrupt_all();

	ptr self = shared_from_this();
	auto it = std::find(clients.begin(), clients.end(), self);
	clients.erase(it);
	BOOST_LOG_SEV(cia_g_logger, Debug) << "客户端socket已经调用stop函数关闭";
	//x m_config_server->set_started(true);	// 为了防止网络情况异常, 造成服务端关闭连接后重置此值为2, 通讯端保证此值为1
}

void cia_client::do_read_header()
{
	//boost::shared_ptr<chat_message> ch_msg_ = m_chat_msg_queue.Take();
	boost::shared_ptr<chat_message> ch_msg_ = boost::make_shared<chat_message>();
	ptr self = shared_from_this();
	boost::asio::async_read(m_sock_,
		boost::asio::buffer(ch_msg_->data(), chat_message::header_length),
		[this, self, ch_msg_](boost::system::error_code ec, std::size_t /*length*/)
	{
		if (!ec && ch_msg_->decode_header())
		{
			//	BOOST_LOG_SEV(cia_g_logger, Debug) << "接收新的数据, 消息体长度: " << ch_msg_->body_length();
			do_read_body(ch_msg_);
		}
		else
		{
			BOOST_LOG_SEV(cia_g_logger, Debug) << "接收新的数据出错, 已经关闭此socket, 错误码:  " << ec;
			stop();
		}
	});
}

inline void cia_client::do_read_body(boost::shared_ptr<chat_message> ch_msg_)
{
	ptr self = shared_from_this();
	//BOOST_LOG_SEV(cia_g_logger, Debug) << "开始准备异步读取数据";	
	boost::asio::async_read(m_sock_,
		boost::asio::buffer(ch_msg_->body(), ch_msg_->body_length()),
		[this, self, ch_msg_](boost::system::error_code ec, std::size_t /*length*/)
	{
		if (!ec)
		{
			//BOOST_LOG_SEV(cia_g_logger, Debug) << "已读取新的消息体, 开始进行下一次读取";
			do_read_header();
			m_update_time.start();
			//BOOST_LOG_SEV(cia_g_logger, Debug) << "开始解析本次请求的消息体";
			do_deal_request(ch_msg_);
		}
		else
		{
			BOOST_LOG_SEV(cia_g_logger, Debug) << "接收新的数据出错, 已经关闭此socket, 错误码:" << ec;
			stop();
		}
	});
}

inline void cia_client::do_write(boost::shared_ptr<chat_message> ch_msg)
{
	//BOOST_LOG_SEV(cia_g_logger, Debug) << "当前m_chat_msg_queue队列剩余元素:" << m_chat_msg_queue.Size();
	ptr self = shared_from_this();
	//BOOST_LOG_SEV(cia_g_logger, Debug) << "异步发送的数据为:" << std::endl; //<< ch_msg->m_procbuffer_msg.DebugString();
	//BOOST_LOG_SEV(cia_g_logger, Debug) << "rn id:" << ch_msg->m_procbuffer_msg.transid();
	LOG_MSG_QUEUE.Put(boost::make_shared<LOG_MSG>(Debug, "异步发送的数据为:\n" + ch_msg->m_procbuffer_msg.DebugString()));
	if (!ch_msg->parse_to_cia_msg())
	{		
		BOOST_LOG_SEV(cia_g_logger, Debug) << "解析待发送的报文出错";
		//m_chat_msg_queue.Put(ch_msg);
		return;
	}
	m_sock_.async_send(boost::asio::buffer(ch_msg->data(), ch_msg->length()),
		[this, self, ch_msg](boost::system::error_code ec, std::size_t /*length*/){		
		if (ec){
			BOOST_LOG_SEV(cia_g_logger, Debug) << "异步发送数据回调函数，检测到异常， 关闭此客户端socket, 错误码" << ec;
			stop();
		}
		else
		{
			//BOOST_LOG_SEV(cia_g_logger, Debug) << "已成功异步发送数据, 数据的transid:" << ch_msg->m_procbuffer_msg.transid();
		}
		//m_chat_msg_queue.Put(ch_msg);
	});
}

void cia_client::do_timeout_check()
{
	if (m_started_)
	{
		m_check_timeout_timer.expires_from_now(boost::posix_time::milliseconds(TIMEOUT_CHECK_ELAPSED));
		ptr self = shared_from_this();
		//BOOST_LOG_SEV(cia_g_logger, AllEvent) << "开始准备异步检测超时, 触发检测的时间是在"
		//	<< TIMEOUT_CHECK_ELAPSED << "毫秒后, " << "客户端socket超时时间设置为" << m_timeout_elapsed << "毫秒";
		m_check_timeout_timer.async_wait([this, self](const error_code& ec){
			if (ec)
			{
				BOOST_LOG_SEV(cia_g_logger, Debug) << "已停止超时检测";
				stop();
				return;
			}
			std::size_t elapsed_time_ = std::size_t(m_update_time.elapsed().wall / 1000000);
			if (elapsed_time_ > m_timeout_elapsed) {
				BOOST_LOG_SEV(cia_g_logger, Debug) << "客户端因超时关闭, 已经在"
					<< elapsed_time_ << "毫秒内无任何动作";
				stop();
			}
			else if (elapsed_time_ > m_timeout_elapsed / 2) {
				BOOST_LOG_SEV(cia_g_logger, AllEvent) << "向客户端发送心跳请求, 已经在"
					<< elapsed_time_ << "毫秒内无任何动作";
				do_deal_heart_request();
			}
			do_timeout_check();
		});
	}
}

inline void cia_client::do_deal_request(boost::shared_ptr<chat_message> ch_msg)
{
	if (!ch_msg->parse_cia_mag()) {
		BOOST_LOG_SEV(cia_g_logger, Debug) << "报文转换失败, 本次请求不做处理";
		//m_chat_msg_queue.Put(ch_msg);
		return;
	}
	LOG_MSG_QUEUE.Put(boost::make_shared<LOG_MSG>(Debug, "接收的数据为:\n" + ch_msg->m_procbuffer_msg.DebugString()));
	//BOOST_LOG_SEV(cia_g_logger, Debug) << "get id:" << ch_msg->m_procbuffer_msg.transid()
	//	<< ",cn:" << ch_msg->m_procbuffer_msg.authcode(); //std::endl;// << ch_msg->m_procbuffer_msg.DebugString();
	if (ch_msg->m_procbuffer_msg.type() == CIA_LOGIN_REQUEST){
		BOOST_LOG_SEV(cia_g_logger, Debug) << "本次请求判断为登陆请求";
		do_deal_login_request(ch_msg);
	}
	else if (ch_msg->m_procbuffer_msg.type() == CIA_HEART_RESPONSE){
		//m_chat_msg_queue.Put(ch_msg);
		BOOST_LOG_SEV(cia_g_logger, AllEvent) << "本次请求判断为心跳回应, 已对客户端的最后连接时间做更新";
		m_update_time.start();
	}
	else if (ch_msg->m_procbuffer_msg.type() == CIA_CALL_REQUEST){
		//BOOST_LOG_SEV(cia_g_logger, Debug) << "本次请求判断为呼叫请求";
		do_deal_call_out_request(ch_msg);
	}
}

inline void cia_client::do_deal_call_out_request(boost::shared_ptr<chat_message> ch_msg)
{
	ptr self = shared_from_this();
	m_base_voice_card->cti_callout(boost::make_shared<cti_call_out_param>(self, ch_msg, true));
}

void cia_client::do_deal_heart_request()
{
	boost::shared_ptr<chat_message> ch_msg = boost::make_shared<chat_message>();
	ch_msg->m_procbuffer_msg.Clear();
	ch_msg->m_procbuffer_msg.set_type(CIA_HEART_REQUEST);
	do_write(ch_msg);
}

void cia_client::do_deal_login_request(boost::shared_ptr<chat_message> ch_msg)
{
	ch_msg->m_procbuffer_msg.Clear();
	ch_msg->m_procbuffer_msg.set_type(CIA_LOGIN_RESPONSE);
	ch_msg->m_procbuffer_msg.set_status(CIA_LOGIN_SUCCESS);
	do_write(ch_msg);
	m_is_login = true;
}

cia_client::~cia_client()
{
	BOOST_LOG_SEV(cia_g_logger, Debug) << "已经对socket析构";
}

#endif // !CIA_CLIENT_HPP_INCLUDE_