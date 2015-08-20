#ifndef BASE_VOICE_CARD_CONTROL_INCLUDE_
#define BASE_VOICE_CARD_CONTROL_INCLUDE_
#include "../system/include_sys.h"
#include "../net_logic/base_client.hpp"
#include "../tools/boost_log.hpp"
#include <boost/asio.hpp>
#include <boost/timer/timer.hpp>

#include <memory>
#include <string>

/**
* \brief 呼叫请求入参结构体
*
* \author LYL QQ-331461049
* \date 2015/08/18 14:15
*/
using namespace boost::chrono;
struct cti_call_out_param
{
	typedef boost::shared_ptr<boost::asio::deadline_timer> cia_timer;
public:
	cti_call_out_param(boost::shared_ptr<base_client> base_client, const std::string& transId, const std::string& authCode, const std::string& pn, bool hungup_by_echo_tone = true)
	{
		m_base_client = base_client;
		m_transId = transId;
		m_authCode = authCode;
		m_pn = pn;
		m_hungup_by_echo_tone = hungup_by_echo_tone;
		m_repeat_call_out = true;
		m_call_time.start();
	}
	int cti_call_out_elapsed_milliseconds()
	{
		return (int)(m_call_time.elapsed().wall)/1000000;
	}
	boost::shared_ptr<base_client> m_base_client;	       // 用于调用do_write函数， 实现对呼叫结果的写入
	std::string                    m_transId;	           // 业务流水
	std::string                    m_authCode;             // 主叫号码
	std::string                    m_pn;                   // 被叫号码
	bool                           m_hungup_by_echo_tone;  // 是否响一声后挂断, 默认true, 生产环境均需做此设置, 调试语音卡的时候, 可以设置false, 外呼不会响一声挂断, 但会被超时检测线程挂断
	cia_timer                      m_repeat_call_out_timer;// 用于重复呼叫的定时器
	bool                           m_repeat_call_out;      // 本次呼叫如果失败，是否继续呼叫，当语音卡通道繁忙导致无法呼叫， 会循环反复呼叫，直到超时。当第一次呼叫失败了，会尝试第二次呼叫， 提高成功率，避免因通道问题造成呼叫失败
private:
	boost::timer::cpu_timer		   m_call_time;            // 发送呼叫请求计时
};

/**
* \brief 语音卡基类， 模拟语音卡操作， 用于单独测试其他组件
*
* \author LYL QQ-331461049
* \date 2015/08/18 13:56
*/
class base_voice_card_control
{
public:
	/**
	* 语音卡外呼函数，由子类覆盖产生实际语音卡调用
	*
	* \param client_socket 用于调用do_write函数， 实现对呼叫结果的写入
	* \param transId 业务流水
	* \param authCode 主叫号码
	* \param pn 被叫号码
	* \param hungup_by_echo_tone 是否响一声后挂断, 默认true, 生产环境均需做此设置, 调试语音卡的时候, 可以设置false, 外呼不会响一声挂断, 但会被超时检测线程挂断
	*/
	virtual void cti_callout(boost::shared_ptr<cti_call_out_param> cti_call_out_param_)
	{
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "模拟发送呼叫请求";
		ciaMessage msg;
		msg.set_type(CIA_CALL_RESPONSE);
		msg.set_transid(cti_call_out_param_->m_transId);
		msg.set_status(CIA_CALL_SUCCESS);
		cti_call_out_param_->m_base_client->do_write(chat_message(msg));
	};

	/**
	* \brief 获取语音卡空闲通道数
	*
	* \return 返回语音卡空闲通道数
	*/
	virtual std::size_t get_idol_channel_number(){
		return 100000;
	}
protected:
private:
};

#endif // !BASE_VOICE_CARD_CONTROL_INCLUDE_