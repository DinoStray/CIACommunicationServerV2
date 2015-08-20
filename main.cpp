#include "system/include_sys.h"
#include "tools/config_server.hpp"
#include "net_logic/cia_server.hpp"
#include "cti/voice_card_control.hpp"

#include <stdlib.h>
#include <exception>
#include <iostream>
void testNetLogic(boost::shared_ptr<config_server> config_server_);
void testCTI(boost::shared_ptr<config_server> config_server_);
void test_all(boost::shared_ptr<config_server> config_server_);
void test_config_server(boost::shared_ptr<config_server> config_server_);

int main(int argc, char* argv[]) {
	if (argc < 4)
	{
		std::cout << "请按以下方式运行: 可执行文件名 current_node_path zookeeper_server_ip_port log_config_file" << std::endl
			<< "current_node_path 一般为/auth/ip, ip为当前通讯端服务器的IP地址, 如/auth/116.255.130.148" << std::endl
			<< "zookeeper_server_ip_port 逗号分隔的 IP:端口 标识, 每个关联一个 zookeeper 服务端, 如:127.0.0.1:3000,127.0.0.1:3001,127.0.0.1:3002"
			<< "log_config_file 日志配置文件"
			<< std::endl;
		return -1;
	}
	try{
		init_log(argv[3]);
	}
	catch (std::exception& e){
		std::cout << e.what() << std::endl;
		return -1;
	}
	boost::shared_ptr<config_server> config_server_ = boost::make_shared<config_server>(argv[1], argv[2]);
	testNetLogic(config_server_);
	//testCTI(config_server_);
	//test_all(config_server_);
	//test_config_server(config_server_);
	boost::this_thread::sleep_for(boost::chrono::seconds(3));
	config_server_->set_idol_channel_number(0);
	config_server_->set_started(false);
}

void test_all(boost::shared_ptr<config_server> config_server_)
{
	boost::shared_ptr<voice_card_control> p_vcc = boost::make_shared<voice_card_control>(config_server_, true);
	boost::shared_ptr<cia_server> cs = boost::make_shared<cia_server>(config_server_, p_vcc);
	cs->start();
	config_server_->set_started(true);
	std::string readLine;
	while (true){
		std::cin >> readLine;
		if (readLine == "quit")
		{
			break;
		}
		boost::this_thread::sleep_for(boost::chrono::seconds(1));
	};
}

void testNetLogic(boost::shared_ptr<config_server> config_server_)
{
	boost::shared_ptr<base_voice_card_control> p_vcc = boost::make_shared<base_voice_card_control>();
	boost::shared_ptr<cia_server> cs = boost::make_shared<cia_server>(config_server_, p_vcc);
	cs->start();
	config_server_->set_started(true);
	std::string readLine;
	while (true){
		std::cin >> readLine;
		if (readLine == "quit")
		{
			break;
		}
		boost::this_thread::sleep_for(boost::chrono::seconds(1));
	};
}

void testCTI(boost::shared_ptr<config_server> config_server_)
{
	try
	{
		// 78  服务器 86051200 86051882 86051822
		// 145 服务器 86057405 86057408 86057410	86057415 86057423 86057428
		//            86057431 86057435 86057437 86057459 86057501 86057851 86057861
		// 148 服务器 65992764 35993934
		std::string callerNum = "86051200";
		// 李禹霖 018072710179 018515663997
		// 马超   018611967787
		std::string calledNum = "018515663997";
		boost::shared_ptr<voice_card_control> p_vcc = boost::make_shared<voice_card_control>(config_server_, true);
		size_t trans_id = 100;
		boost::shared_ptr<base_client> client_ptr = boost::make_shared<base_client>();
		BOOST_LOG_SEV(cia_g_logger, Critical) << ">>>>>--------------------------------------------------------------------第一波检测, 测试10次呼叫, 响一声挂断--------------------------------------------------------------------<<<<<";
		for (size_t i = 0; i < 10; i++)
		{
			p_vcc->cti_callout(boost::make_shared<cti_call_out_param>(client_ptr, std::to_string(trans_id++), callerNum, calledNum));
			boost::this_thread::sleep_for(boost::chrono::seconds(20));
		}
		//callerNum = "86051882";
		//BOOST_LOG_SEV(g_logger, Critical) << ">>>>>--------------------------------------------------------------------第二波检测, 测试10次呼叫, 响一声不挂断, 等待超时15秒后, 由超时检测线程挂断--------------------------------------------------------------------<<<<<";
		//for (size_t i = 0; i < 10; i++)
		//{
		//	vcc.cti_callout(p, to_string(trans_id++), callerNum, calledNum, false);
		//	boost::this_thread::sleep_for(boost::chrono::seconds(20));
		//}
	}
	catch (std::exception &e)
	{
		std::cout << "程序异常:" << e.what() << std::endl;
	}
}

void test_config_server(boost::shared_ptr<config_server> config_server_)
{
	std::cout << "get_client_socket_timeout_elapsed:" << config_server_->get_client_socket_timeout_elapsed() << std::endl;
	std::cout << "get_cti_sleeping_elapsed:" << config_server_->get_cti_sleeping_elapsed() << std::endl;
	std::cout << "get_cti_timeout_elapsed:" << config_server_->get_cti_timeout_elapsed() << std::endl;
	std::cout << "get_cti_total_channel_count:" << config_server_->get_cti_total_channel_count() << std::endl;
	std::cout << "get_cti_warning_elapsed:" << config_server_->get_cti_warning_elapsed() << std::endl;
	std::cout << "get_iocp_thread_number:" << config_server_->get_iocp_thread_number() << std::endl;
	std::cout << "get_server_port:" << config_server_->get_server_port() << std::endl;
	std::cout << "get_cti_set_idol_channel_num_elapsed:" << config_server_->get_cti_set_idol_channel_num_elapsed() << std::endl;
	config_server_->set_idol_channel_number(10);
	config_server_->set_started(true);
}