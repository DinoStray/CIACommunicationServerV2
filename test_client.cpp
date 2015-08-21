#include "system/include_sys.h"
#include "tools/boost_log.hpp"
#include "tools/blocking_queue.hpp"
#include "net_logic/chat_message.hpp"
#include "net_logic/cia_def.h"
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/random.hpp>

using namespace boost::asio;
//生产环境
//const std::string server_ip = "60.216.6.145";
//本机
//const std::string server_ip = "127.0.0.1";
//济南 78 服务器
//const std::string server_ip = "119.164.213.182";
const int server_port = 8999;

boost::random::mt19937 gen;
boost::uniform_int<> real(-9999999, 9999999);

class cia_client_socket :
	public boost::enable_shared_from_this<cia_client_socket>,
	public boost::noncopyable
{
	typedef cia_client_socket self_type;
	typedef boost::system::error_code error_code;
	typedef boost::shared_ptr<cia_client_socket> ptr;
public:
	cia_client_socket(io_service& io_service_) :
		m_socket(io_service_)
	{
		int write_msg_queue_size = 24;
		while (write_msg_queue_size--)
		{
			m_write_msg_queue_.Put(boost::make_shared<chat_message>());
		}
	};
	void batch_send_msg(int batch_count);
	bool connect_to_service(std::string server_ip_, int server_port_);

protected:
	void do_read_header();
	void do_read_body();
	void do_write(chat_message ch_msg);
private:
	ip::tcp::socket m_socket;
	chat_message m_read_msg_;
	blocking_queue<boost::shared_ptr<chat_message>> m_write_msg_queue_;
};

bool cia_client_socket::connect_to_service(std::string server_ip_, int server_port_)
{
	ip::tcp::endpoint endpoint_(ip::address::from_string(server_ip_), server_port);
	try
	{
		m_socket.connect(endpoint_);
	}
	catch (const boost::system::system_error& ec)
	{
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "与服务器连接失败，错误:" << ec.what();
		return false;
	}
	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "已成功和服务器连接";
	do_read_header();
	return true;
}

void cia_client_socket::do_read_header()
{
	ptr self = shared_from_this();
	boost::asio::async_read(m_socket,
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
		}
	});
}

void cia_client_socket::do_read_body()
{
	ptr self = shared_from_this();
	BOOST_LOG_SEV(cia_g_logger, Debug) << "开始准备异步读取数据";
	boost::asio::async_read(m_socket,
		boost::asio::buffer(m_read_msg_.body(), m_read_msg_.body_length()),
		[this, self](boost::system::error_code ec, std::size_t /*length*/)
	{
		if (!ec)
		{
			chat_message ch_msg = m_read_msg_;
			BOOST_LOG_SEV(cia_g_logger, Debug) << "已读取新的消息体, 开始进行下一次读取";
			//do_read_header();
			ciaMessage msg;
			if (!msg.ParseFromArray(ch_msg.body(), ch_msg.body_length())) {
				BOOST_LOG_SEV(cia_g_logger, Debug) << "报文转换失败, 本次请求不做处理";
				return;
			}
			BOOST_LOG_SEV(cia_g_logger, Debug) << "本次请求的消息体内容为: " << std::endl << msg.DebugString();
		}
		else
		{
			BOOST_LOG_SEV(cia_g_logger, Debug) << "接收新的数据出错, 已经关闭此socket, 错误码:  " << ec;
		}
	});
}

void cia_client_socket::do_write(chat_message ch_msg)
{
	boost::shared_ptr<chat_message> _ch_msg = m_write_msg_queue_.Take();
	BOOST_LOG_SEV(cia_g_logger, Debug) << "当前m_write_msg_queue_队列剩余元素:" << m_write_msg_queue_.Size();
	*_ch_msg = ch_msg;
	ptr self = shared_from_this();
	BOOST_LOG_SEV(cia_g_logger, Debug) << "开始准备异步发送数据";
	BOOST_LOG_SEV(cia_g_logger, Debug) << "异步发送的数据为::" << std::endl << ch_msg.m_procbuffer_msg.DebugString();
	m_socket.async_send(boost::asio::buffer(_ch_msg->data(), _ch_msg->length()),
		[this, self, _ch_msg](boost::system::error_code ec, std::size_t /*length*/){
		m_write_msg_queue_.Put(_ch_msg);
		if (ec){
			BOOST_LOG_SEV(cia_g_logger, Debug) << "异步发送数据回调函数，检测到异常， 关闭此客户端socket, 错误码" << ec;
		}
		else
		{
			BOOST_LOG_SEV(cia_g_logger, Debug) << "已成功异步发送数据, 数据的transid:" << _ch_msg->m_procbuffer_msg.transid();
			do_read_header();
		}
	});
}

void cia_client_socket::batch_send_msg(int batch_count)
{

	while (batch_count--)
	{
		ciaMessage msg;
		msg.set_type("010103");
		msg.set_transid(std::to_string(real(gen)));
		msg.set_authcode("86057365");
		msg.set_pn("018515663997");
		do_write(msg);
	}
}

int main(int argc, char* argv[])
{
	std::vector<boost::shared_ptr<cia_client_socket>> client_vector;
	try{
		init_log(argv[1]);
	}
	catch (std::exception& e){
		std::cout << e.what() << std::endl;
		return -1;
	}
	io_service io_service_;
	io_service::work dummy_work_(io_service_);
	boost::thread_group thread_group_;
	int i = 24;
	while (i--)
	{
		thread_group_.create_thread([&](){
			io_service_.run();
		});
	}
	int client_count = std::stoi(argv[4]);
	while (client_count--)
	{
		client_vector.push_back(boost::make_shared<cia_client_socket>(io_service_));
	}
	int each_thread_num = std::stoi(argv[3]) / std::stoi(argv[4]);
	for (auto p : client_vector)
	{
		if (p->connect_to_service(argv[2], server_port))
		{
			//int i = 5;
			//int each_thread_num = std::stoi(argv[3]) / i;
			//while (i--)
			//{
			//	thread_group_.create_thread([&](){
			//		client_socket_->batch_send_msg(each_thread_num);
			//	});
			//}
			thread_group_.create_thread([&](){
				p->batch_send_msg(each_thread_num);
			});
			
		}
	}
	
	system("pause");
	io_service_.stop();
	return 0;
}