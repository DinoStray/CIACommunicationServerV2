#ifndef CTI_TRUNK_HPP_INCLUDED
#define CTI_TRUNK_HPP_INCLUDED
#include "../system/include_sys.h"
#include "../net_logic/base_client.hpp"

#include <string>
#include <memory>

#include <boost/timer.hpp>
#include <boost/thread.hpp>
#include <boost/timer/timer.hpp>

/**
* 语音卡通道状态描述
*
* @author LYL QQ-331461049
* @date 2015/07/16 18:28
*/
enum trunk_state {
	TRK_IDLE,	                            // 空闲
	TRK_WAIT_CONNECT,	                    // 被占用, 等待连接
	TRK_CALLOUT_DAIL,	                    // 已拨号
	TRK_CHEK_BARGEIN,	                    // 检测铃音
	TRK_SLEEP,	                            // 延迟挂机状态
	TRK_HUNGUP                              // 准备挂机状态
};

/**
* 每个语音卡， 都会有 （中继线数量 * 30） 个通道， 每次呼叫都会占用一个通道
* 此类保存每次呼叫时， 语音卡通道的相关信息
*
* @author LYL QQ-331461049
* @date 2015/07/16 18:35
*/
class trunk
{
private:
	boost::timer::cpu_timer        m_callTime;	           // 发起呼叫的时间
public:
	trunk_state                    m_step;	               // 当前通道的状态  //
	std::string                    m_caller_id;            // 主叫号码*
	std::string                    m_called_id;	           // 被叫号码*
	std::string                    m_transId;              // 业务流水号*
	bool                           m_hungup_by_echo_tone;  // 是否响一声挂机, false 非响一声挂机情况仅限于测试用, 生产环境设置为 true, 保证响一声立即挂机*
	boost::mutex                   m_trunk_mutex;          // 通道状态锁
	boost::shared_ptr<base_client> m_client_socket;	       // 每次呼叫时，需要配合其他组件保存的信息*

	trunk()
	{
		m_step = TRK_IDLE;
	}
	
	/**
	 *\brief 初始化trunk类
	 *
	 */
	void init(const std::string& caller_id,const std::string& called_id,const std::string& transId, bool hungup_by_echo_tone, boost::shared_ptr<base_client> client_socket)
	{
		m_step = TRK_IDLE;
		this->m_caller_id = caller_id;
		this->m_called_id = called_id;
		this->m_transId = transId;
		this->m_hungup_by_echo_tone = hungup_by_echo_tone;
		this->m_client_socket = client_socket;
		m_callTime.start();
	}
	/**
	* \brief 获取通道被占用的时间
	*
	* \return 返回 获取通道被占用的时间， 单位：毫秒
	*/
	int elpased()
	{
		return (int)(m_callTime.elapsed().wall / 1000000);
	}

	/**
	* 每次呼叫完毕， 需要重置相关资源
	*
	*/
	void realseTrunk()
	{
		m_step = TRK_IDLE;
		m_caller_id.clear();
		m_called_id.clear();
		m_transId.clear();
		m_callTime.start();
		m_hungup_by_echo_tone = true;
		m_client_socket.reset();
	}
};

#endif