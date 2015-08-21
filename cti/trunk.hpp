#ifndef CTI_TRUNK_HPP_INCLUDED
#define CTI_TRUNK_HPP_INCLUDED
#include "../system/include_sys.h"
#include "../net_logic/base_client.hpp"

#include <string>
#include <memory>

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
	boost::timer::cpu_timer               m_callTime;	            // 发起呼叫的时间
public:
	trunk_state                           m_step;	                // 当前通道的状态  
	boost::shared_ptr<cti_call_out_param> m_call_out_param;         // 保存本次呼叫相关信息
	boost::mutex                          m_trunk_mutex;            // 通道锁

	trunk()
	{
		m_step = TRK_IDLE;
	}

	/**
	 *\brief 使用此通道外呼后对乘员变量赋值
	 *
	 */

	void reset_trunk(boost::shared_ptr<cti_call_out_param> call_out_param_)
	{
		m_step = TRK_CALLOUT_DAIL;
		m_call_out_param = call_out_param_;
		m_callTime.start();
	}
	/**
	* \brief 获取通道被占用的时间
	*
	* \return 返回 获取通道被占用的时间， 单位：毫秒
	*/
	std::size_t elpased()
	{
		return (std::size_t)(m_callTime.elapsed().wall / 1000000);
	}

	/**
	* 每次呼叫完毕， 需要重置相关资源
	*
	*/
	void realseTrunk()
	{
		m_step = TRK_IDLE;
		m_callTime.start();
		m_call_out_param.reset();
	}
};

#endif