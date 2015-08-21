#ifndef BASE_CLIENT_HPP_INCLUDE
#define BASE_CLIENT_HPP_INCLUDE

#include "../system/include_sys.h"
#include "chat_message.hpp"
#include "../tools/boost_log.hpp"
class base_client
{
public:
	virtual void do_write(boost::shared_ptr<chat_message> ch_msg)
	{
		BOOST_LOG_SEV(cia_g_logger, Debug) << "模拟客户端socket发送呼叫结果";
	};
	base_client(){};
	~base_client(){};
protected:
private:
};

#endif // !BASE_CLIENT_HPP_INCLUDE