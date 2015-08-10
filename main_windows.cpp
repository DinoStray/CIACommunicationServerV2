#include "tools\boost_log.hpp"
#include "net_logic\cia_server.hpp"
#include "cti\voice_card_control.hpp"
#include "windows\include_win.h"

#include <stdlib.h>
#include <exception>
#include <iostream>
void testNetLogic();
void testCTI();
void test_all();

int main(int argc, char* argv[]) {
	try{
		init_log();
	}
	catch (std::exception& e){
		std::cout << e.what() << std::endl;
		return -1;
	}
	//testNetLogic();
	//testCTI();
	test_all();
	::system("pause");
}

void test_all()
{
	boost::shared_ptr<voice_card_control> p_vcc = boost::make_shared<voice_card_control>(30, 15, true);
	cia_server cs(16, 8999, p_vcc, 0);
	std::string readLine;
	while (true){
		std::cin >> readLine;
		if (readLine == "quit")
		{
			break;
		}
		Sleep(1000);
	};
}

void testNetLogic()
{
	boost::shared_ptr<base_voice_card_control> p_vcc = boost::make_shared<base_voice_card_control>();
	cia_server cs(16, 8999, p_vcc, 15000);
	std::string readLine;
	while (true){
		std::cin >> readLine;
		if (readLine == "quit")
		{
			break;
		}
		Sleep(1000);
	};
}

void testCTI()
{
	try
	{
		// 78  ������ 86051200 86051882 86051822
		// 145 ������ 86057405 86057408 86057410	86057415 86057423 86057428
		//           86057431 86057435 86057437 86057459 86057501 86057851 86057861
		string callerNum = "86051200";
		// ������ 018072710179 018515663997
		// ��   018611967787
		string calledNum = "018515663997";
		voice_card_control vcc(30, 15, true);
		size_t trans_id = 100;
		boost::shared_ptr<base_client> client_ptr = boost::make_shared<base_client>();
		BOOST_LOG_SEV(cia_g_logger, Critical) << ">>>>>--------------------------------------------------------------------��һ�����, ����10�κ���, ��һ���Ҷ�--------------------------------------------------------------------<<<<<";
		for (size_t i = 0; i < 10; i++)
		{
			vcc.cti_callout(client_ptr, to_string(trans_id++), callerNum, calledNum);
			Sleep(10 * 1000);
		}
		//callerNum = "86051882";
		//BOOST_LOG_SEV(g_logger, Critical) << ">>>>>--------------------------------------------------------------------�ڶ������, ����10�κ���, ��һ�����Ҷ�, �ȴ���ʱ15���, �ɳ�ʱ����̹߳Ҷ�--------------------------------------------------------------------<<<<<";
		//for (size_t i = 0; i < 10; i++)
		//{
		//	vcc.cti_callout(p, to_string(trans_id++), callerNum, calledNum, false);
		//	Sleep(20 * 1000);
		//}
	}
	catch (std::exception &e)
	{
		cout << "�����쳣:" << e.what() << endl;
	}
}
