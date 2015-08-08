#include "include_lib.h"
#include "boost_log.hpp"
#include "cia_server.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
	try{
		init_log();
	}
	catch (exception& e){
		std::cout << e.what() << std::endl;
		return -1;
	}
	cia_server cs(16, 8999);

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