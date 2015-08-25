#ifndef CTI_VOICE_CARD_CONTROL_HPP_INCLUDED
#define CTI_VOICE_CARD_CONTROL_HPP_INCLUDED

#include "../system/include_sys.h"
#include "../tools/config_server.hpp"
#include "trunk.hpp"
#include "../tools/thread_safe_queue.hpp"
#include "../net_logic/cia_def.h"
#include "base_voice_card_control.hpp"

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/asio/system_timer.hpp>

#include <exception>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>

/**
 * 语音卡控制类, 用来做外呼控制, 外呼结果判断
 *
 * \author LYL QQ-331461049
 * \date 2015/07/21 19:10
 */
class voice_card_control : public
	base_voice_card_control
{
public:
	static thread_safe_queue<std::size_t>           m_channel_queue;	                // 保存可用的通道号码
	//保存中继状态数组, 由于在trunk中使用了NO_COPYABLE的类型 mutex, 所以此处存储shared_ptr
	static std::vector<boost::shared_ptr<trunk>>        m_trunk_vector;                 // 语音卡通道数组， 保存全部封装后的语音卡通道

	voice_card_control(boost::shared_ptr<config_server> config_server_, bool use_strategy = true);
	~voice_card_control();

	static int CALLBACK cti_callback(WORD wEvent, int nReference, DWORD dwParam, DWORD dwUser);
	static int cti_hangUp(std::size_t channelID, std::string status);
	static int timeout_check();
	virtual void cti_callout(boost::shared_ptr<cti_call_out_param> cti_call_out_param_);
	int cti_callout_by_channel_ID(int channelID, boost::shared_ptr<base_client> CTRUNC_ATTACHMENT, std::string transId, std::string authCode, std::string pn);	//
	virtual std::size_t get_idol_channel_number();

protected:
	static int deal_e_proc_auto_dial(int nReference, DWORD dwParam, DWORD dwUser);
	static int deal_e_rcv_ss7msu(int nReference, DWORD dwParam, DWORD dwUser);
	static int deal_e_sys_bargein(int nReference, DWORD dwParam, DWORD dwUser);
	static void deal_hungup_strategy();
	static void show_error();
	void init_cti();
	void cti_callout_again(boost::shared_ptr<cti_call_out_param> cti_call_out_param_);

private:
	std::size_t                            m_numChannelCount;                       // 语音卡通道总数
	static std::size_t                     m_timeout_elapse;                        // 呼叫超时强制挂断的时间, 单位秒
	static thread_safe_queue<std::size_t>  m_sleep_channel_queue;	                // 保存因延迟策略, 需要Sleep的通道号码
	boost::thread                          m_timeout_thread;                        // 语音通道占用超时检测
	boost::thread_group                    m_hungup_strategy_thread_group;          // 执行延迟策略的线程池
	boost::thread_group                    m_io_service_thread_group;               // 执行延迟策略的线程池
	boost::shared_ptr<config_server>       m_config_server;                         // 配置服务对象
	static bool                            m_use_strategy;                          // 是否使用延迟策略
	static std::size_t                     m_cti_warning_elapse;	                // 触发策略的呼叫响应间隔时间。 发起呼叫后， 如果在设定的时间内触发响铃， 则属于“超前”响铃时间， 需要延迟挂机
	static std::size_t                     m_cti_sleeping_elapse;                   // 触发策略后， 延迟多长时间挂机
	boost::asio::io_service                m_io_service_;                           // io_service 对象， 用于和deadline_timer配合做定时操作
	boost::asio::io_service::work          m_dummy_worker;                          // 为了维持io_service线程池持续运行, 而设置的虚拟任务
};

std::vector<boost::shared_ptr<trunk>>  voice_card_control::m_trunk_vector;          // 保存中继状态数组, 由于在trunk中使用了NO_COPYABLE的类型 mutex, 所以此处存储shared_ptr
thread_safe_queue<std::size_t>         voice_card_control::m_channel_queue;         // 保存可用的通道号码
std::size_t                            voice_card_control::m_timeout_elapse;        // 呼叫超时强制挂断的时间, 单位秒
thread_safe_queue<std::size_t>         voice_card_control::m_sleep_channel_queue;   // 保存因延迟策略, 需要Sleep的通道号码
bool                                   voice_card_control::m_use_strategy;          // 是否使用延迟策略
std::size_t                            voice_card_control::m_cti_warning_elapse;    // 触发策略的呼叫响应间隔时间。 发起呼叫后， 如果在设定的时间内触发响铃， 则属于“超前”响铃时间， 需要延迟挂机
std::size_t                            voice_card_control::m_cti_sleeping_elapse;   // 触发策略后， 延迟多长时间挂机
/**
* \brief 初始化语音卡, 日志等信息
*
* \param numChannelCount 语音卡通道数
* \param timeout_elapse 呼叫超时时间, 单位秒
* \param use_strategy 是否加载延迟策略
* \param 是否把呼叫结果放入队列, 方便其他组件获取
*/
voice_card_control::voice_card_control(boost::shared_ptr<config_server> config_server_, bool use_strategy)
: m_timeout_thread(voice_card_control::timeout_check), m_hungup_strategy_thread_group(), m_io_service_thread_group(),
m_config_server(config_server_),
m_io_service_(), m_dummy_worker(m_io_service_) //, m_timer(m_io_service_)
{
	m_numChannelCount = m_config_server->get_cti_total_channel_count();
	m_timeout_elapse = m_config_server->get_cti_timeout_elapsed();
	m_cti_warning_elapse = m_config_server->get_cti_warning_elapsed();
	m_cti_sleeping_elapse = m_config_server->get_cti_sleeping_elapsed();
	m_use_strategy = use_strategy;

	for (size_t i = 0; i < m_numChannelCount; i++)
	{
		m_trunk_vector.push_back(boost::make_shared<trunk>());
	}
	init_cti();
	//创建延迟策略线程池
	for (size_t i = 0; i <= m_numChannelCount / 2; i++)
	{
		m_hungup_strategy_thread_group.create_thread(deal_hungup_strategy);
	}
	//创建io_service执行线程池， 用于执行定时重复呼叫
	//TODO 未来计划将延迟策略、通道超时也放入这里
	int thread_num = 5; // TODO 修改为 config_server 配置， 当把延迟策略、通道超时也放入这里后， 此值需加倍
	while (thread_num--)
	{
		m_io_service_thread_group.create_thread([this](){
			m_io_service_.run();
		});
	}
	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "语音卡初始化完毕";
}

voice_card_control::~voice_card_control()
{
	m_timeout_thread.interrupt();
	m_hungup_strategy_thread_group.interrupt_all();
	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "语音通道超时检测线程已关闭";
	SsmCloseCti();
	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "语音卡组件已关闭";
	m_io_service_.stop();
	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "IO_SERVICE线程池已关闭";
}

void voice_card_control::init_cti()
{
	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "读取语音卡配置文件";
	if (SsmStartCti("ShConfig.ini", "ShIndex.ini") != 0)
	{
		show_error();
		BOOST_LOG_SEV(cia_g_logger, Critical) << "读取语音卡配置文件失败";
		throw std::runtime_error("读取语音卡配置文件失败");
	}
	else{
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "语音卡配置文件读取成功";
	}
	//设置驱动程序抛出事件的模式

	EVENT_SET_INFO EventMode;
	EventMode.dwWorkMode = EVENT_CALLBACK;                               // 事件回调模式
	EventMode.lpHandlerParam = (LPVOID)voice_card_control::cti_callback; // 注册回调函数

	SsmSetEvent(0xffff, -1, true, &EventMode);	                         // 启动事件触发模式

	//如果要在程序中获取SS7消息, 则需要同时注意以下两点
	//配置文件 ShConfig.ini 中, GetMsuOnAutoHandle = 1
	//程序中: SsmSetEvent(E_RCV_Ss7Msu, -1, true, &EventMode);
	SsmSetEvent(E_RCV_Ss7Msu, -1, true, &EventMode);

	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "正在初始化语音卡通道, 预期耗时15秒";
	//	为了兼容1号信令，注释以下代码，改为sleep 15秒
	//while (SsmSearchIdleCallOutCh(160, 0x1E0000) < 0){	                 // 循环等待, 直到能够获取语音卡空闲通道号, 语音卡初始化完毕
	//	boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
	//}
	boost::this_thread::sleep_for(boost::chrono::seconds(15));
	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "语音卡通道初始化完毕";
	for (std::size_t i = 0; i < m_numChannelCount; i++){
		m_channel_queue.put(i);
		SsmBlockRemoteCh(i);	                                         // 设置通道远端堵塞（禁止来话，但仍可以呼出）
	}
}

/**
 * 显示语音卡错误
 *
 */
void voice_card_control::show_error()
{
	char buff[5000];
	SsmGetLastErrMsg(buff);
	BOOST_LOG_SEV(cia_g_logger, Warning) << buff;
}

/**
 * \brief 超时检测线程
 *
 */
int voice_card_control::timeout_check()
{
	BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "语音通道超时检测线程已开启";
	while (true)
	{
		//既用来休眠100毫秒, 同时用来做 interrupt() 函数的 中断点, 无此函数调用, interrupt() 函数无效		
		boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
		for (size_t i = 0; i < m_trunk_vector.size(); i++)
		{		
			boost::shared_ptr<trunk> trunk = m_trunk_vector.at(i);
			boost::unique_lock<boost::mutex> unique_lock_(trunk->m_trunk_mutex, boost::defer_lock);
			if (trunk->elpased() >= m_timeout_elapse){
				if (!unique_lock_.try_lock())
				{
					continue;
				}
				if (trunk->m_step != TRK_IDLE && trunk->m_call_out_param != nullptr)
				{
					// 如果当前语音卡组件为非响一声挂机， 则m_hungup_by_echo_tone值为false， 在cti_hangUp函数中不会做挂断处理， 所以在此处改值为true
					if (!trunk->m_call_out_param->m_hungup_by_echo_tone){
						trunk->m_call_out_param->m_hungup_by_echo_tone = true;
					}
					BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << trunk->m_call_out_param->m_ch_msg->m_procbuffer_msg.transid() << " 语音通道超时, 强制挂断处理, 语音卡通道号 : " << i;
					unique_lock_.unlock();
					cti_hangUp(i, CIA_CALL_FAIL);
				}
			}
		}
	}
}

/**
 * 语音卡外呼函数
 *
 * \param client_socket, 用于调用do_write函数， 实现对呼叫结果的写入
 * \param transId 业务流水
 * \param authCode 主叫号码
 * \param pn 被叫号码
 * \param hungup_by_echo_tone 是否响一声后挂断, 默认true, 生产环境均需做此设置, 调试语音卡的时候, 可以设置false, 外呼不会响一声挂断, 但会被超时检测线程挂断
 */

void voice_card_control::cti_callout(boost::shared_ptr<cti_call_out_param> cti_call_out_param_)
{
	std::size_t chID;
	ciaMessage& msg_ = cti_call_out_param_->m_ch_msg->m_procbuffer_msg;
	try
	{
		chID = m_channel_queue.take();
	}
	catch (std::out_of_range)
	{
		//BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << msg_.transid() << " 获取通道失败, 通道全部繁忙";
		//继续下一次呼叫
		cti_callout_again(cti_call_out_param_);
		return;
	}
	int ch_state = SsmChkAutoDial(chID);
	if (ch_state != 0)
	{
		//BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << msg_.transid() << " , 获取到得通道状态为:" << ch_state << ", 不可用，将此通道重新放回。通道号码:" << chID;
		m_channel_queue.put(chID);
	}
	//BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << msg_.transid() << ", 获取到的通道号码:" << chID;
	SsmSetTxCallerId(chID, msg_.authcode().c_str());
	boost::shared_ptr<trunk> t = m_trunk_vector.at(chID);
	boost::unique_lock<boost::mutex> unique_lock_(t->m_trunk_mutex);
	if (SsmAutoDial(chID, msg_.pn().c_str()) == 0){
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << msg_.transid() << " 已发送请求, 已将此通道对应状态清空, 通道号码:" << chID;				
		t->reset_trunk(cti_call_out_param_);
	}
	else {
		unique_lock_.unlock();
		m_channel_queue.put(chID);
		//上一次呼叫失败，继续呼叫
		if (cti_call_out_param_->m_repeat_call_out)
		{
			cti_call_out_param_->m_repeat_call_out = false;
			BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << msg_.transid() << "上一次呼叫失败，继续呼叫";
			cti_callout_again(cti_call_out_param_);
		}
		//已经连续两次呼叫失败， 直接返回失败
		else
		{
			BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << msg_.transid() << "已经连续两次呼叫失败， 直接返回失败";
			std::string transid_ = msg_.transid();
			msg_.Clear();
			msg_.set_type(CIA_CALL_RESPONSE);
			msg_.set_transid(transid_);
			msg_.set_status(CIA_CALL_FAIL);
			cti_call_out_param_->m_base_client->do_write(cti_call_out_param_->m_ch_msg);
			return;
		}
	}
}
/**
 * \brief 当第一次呼叫失败， 开始尝试继续呼叫
 *
 * \param repeat_call_out true 当语音卡通道繁忙导致无法呼叫， 会循环反复呼叫，直到超时
 *						  false 当第一次呼叫失败了，会尝试第二次呼叫， 提高成功率，避免因通道问题造成呼叫失败
 */
void voice_card_control::cti_callout_again(boost::shared_ptr<cti_call_out_param> cti_call_out_param_)
{
	ciaMessage& msg_ = cti_call_out_param_->m_ch_msg->m_procbuffer_msg;
	// BOOST_LOG_SEV(cia_g_logger, Debug) << "业务流水:" << msg_.transid() << ", 重复呼叫，已经历时:" << cti_call_out_param_->cti_call_out_elapsed_milliseconds();
	if (cti_call_out_param_->cti_call_out_elapsed_milliseconds() > 25000) //TODO 将此配置放在config_server对象中
	{
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << msg_.transid() << " 呼叫请求超时";
		std::string transid_ = msg_.transid();
		msg_.Clear();
		msg_.set_type(CIA_CALL_RESPONSE);
		msg_.set_transid(transid_);
		msg_.set_status(CIA_CALL_TIMEOUT);
		cti_call_out_param_->m_base_client->do_write(cti_call_out_param_->m_ch_msg);
		return;
	}
	if (cti_call_out_param_->m_repeat_call_out_timer == nullptr)
	{
		cti_call_out_param_->m_repeat_call_out_timer = boost::make_shared<boost::asio::deadline_timer>(m_io_service_);
	}
	cti_call_out_param_->m_repeat_call_out_timer->expires_from_now(boost::posix_time::millisec(5000)); //TODO 将此配置放在config_server对象中
	cti_call_out_param_->m_repeat_call_out_timer->async_wait([this, cti_call_out_param_](const boost::system::error_code& ec){
		ciaMessage& msg_ = cti_call_out_param_->m_ch_msg->m_procbuffer_msg;
		if (!ec)
		{
			BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << msg_.transid() << ", 开始重复呼叫";
			cti_callout(cti_call_out_param_);
		}
		else
		{
			BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << msg_.transid() << ", 重新呼叫的timer出现异常， 异常码：" << ec;
		}
	});
}

/**
 * 挂机函数, 不论触发什么条件导致需要挂机, 都统一在此函数中处理
 *
 * \param channelID 语音卡逻辑通道号码
 * \param status 挂机后, 需要反馈的状态码, 呼叫成功, 用户占线等
 * \return -1 挂机失败, 可能是语音卡反馈失败, 可以调用show_error函数查询失败原因, 也可能是程序逻辑错误, 导致重复挂机, 为避免重复释放资源, 不做处理并返回-1
 *          0 挂机成功
 */

int voice_card_control::cti_hangUp(std::size_t channelID, std::string status)
{
	int retVal = 0;
	boost::shared_ptr<trunk> t = m_trunk_vector.at(channelID);
	boost::unique_lock<boost::mutex> unique_lock_(t->m_trunk_mutex, boost::defer_lock);
	if (!unique_lock_.try_lock())
	{
		BOOST_LOG_SEV(cia_g_logger, Warning) << "cti_hangUp函数尝试锁定通道失败， 放弃挂机， 当前通道Step值为：" << t->m_step;
		return -1;
	}
	else
	{
		switch (t->m_step)
		{
		case TRK_IDLE:
			BOOST_LOG_SEV(cia_g_logger, Warning) << "依据当前通道状态判断可能出现重复挂机, 请在后续版本调查原因, 通道号码:" << channelID;
			return -1;
		case TRK_SLEEP:
			BOOST_LOG_SEV(cia_g_logger, Warning) << "当前通道正在延迟挂机, 通道号码:" << channelID;
			return -1;
		default:
			break;
		}
		if (!t->m_call_out_param->m_hungup_by_echo_tone)
		{
			BOOST_LOG_SEV(cia_g_logger, CalloutMsg) << "由于设置非响一声挂断, 未挂断本次呼叫请求, 等待超时挂断， 通道号码:" << channelID;
			return -1;
		}
		retVal = SsmHangup(channelID);
		if (retVal == -1){
			show_error();
			BOOST_LOG_SEV(cia_g_logger, Warning) << "挂机失败, 请在后续版本调查原因, 通道号码:" << channelID;
		}
		else{
			BOOST_LOG_SEV(cia_g_logger, CalloutMsg) << "已挂断本次呼叫请求, 通道号码:" << channelID;
		}
		ciaMessage& msg_ = t->m_call_out_param->m_ch_msg->m_procbuffer_msg;
		std::string transid_ = msg_.transid();
		msg_.Clear();
		msg_.set_type(CIA_CALL_RESPONSE);
		msg_.set_transid(transid_);
		msg_.set_status(status);
		if (t->m_call_out_param == nullptr)
		{
			BOOST_LOG_SEV(cia_g_logger, Critical) << "严重异常: 挂机时发现客户端socket为空";
		}
		else
		{
			t->m_call_out_param->m_base_client->do_write(t->m_call_out_param->m_ch_msg);
		}
		t->realseTrunk();
		unique_lock_.unlock();
		m_channel_queue.put(channelID);
	}
	return retVal;
}

int CALLBACK voice_card_control::cti_callback(WORD wEvent, int nReference, DWORD dwParam, DWORD dwUser)
{
	try{
		if (m_trunk_vector.at(nReference)->m_call_out_param == nullptr)
		{
			//BOOST_LOG_SEV(cia_g_logger, AllEvent) << "非呼叫请求导致事件 ";
			return 1;
		}
		switch (wEvent)
		{
		case E_SYS_NoSound:
			//nReference 通道的逻辑编号 dwParam 不使用
			//BOOST_LOG_SEV(cia_g_logger, AllEvent) << "语音卡事件: E_SYS_NoSound 信号音检测器：线路上保持静默, 通道的逻辑编号: " << nReference;
			return 1;
			break;
		case E_RCV_Ss7Msu:
			//在E_RCV_Ss7Msu事件中, 参数 nReference dwUser 均不使用, 函数SsmGetSs7Msu用于取出消息
			//nReference 不使用
			//dwParam 不使用。函数SsmGetSs7Msu用于取出消息
			BOOST_LOG_SEV(cia_g_logger, Debug) << "语音卡事件: E_RCV_Ss7Msu";
			//x return deal_e_rcv_ss7msu(nReference, dwParam, dwUser); 为了程序对1号信令 7号信令的兼容， 暂时去掉对SS7信令的解析
			return 1;
			break;
		case E_SYS_BargeIn:
			//nReference 通道的逻辑编号
			//dwParam BargeIn检测器的检测结果发生变化：0：Barge In消失 1：检测到BargeIn
			BOOST_LOG_SEV(cia_g_logger, Debug) << "业务流水:" << m_trunk_vector.at(nReference)->m_call_out_param->m_ch_msg->m_procbuffer_msg.transid()
				<< " 语音卡事件: E_SYS_BargeIn, 通道的逻辑编号: " << nReference
				<< ", 检测器的检测结果发生变化 : " << (dwParam == 0 ? "Barge In消失" : "检测到BargeIn");
			return deal_e_sys_bargein(nReference, dwParam, dwUser);
			break;
		case E_PROC_AutoDial:
			//nReference 通道的逻辑编号
			//dwParam AutoDial任务进展值，参数的具体含义请参见函数SsmChkAutoDial的说明
			BOOST_LOG_SEV(cia_g_logger, Debug) << "业务流水:" << m_trunk_vector.at(nReference)->m_call_out_param->m_ch_msg->m_procbuffer_msg.transid()
				<< " 语音卡事件: E_PROC_AutoDial, 通道的逻辑编号:" << nReference << ", CPGAutoDial任务进展值:" << dwParam;
			return deal_e_proc_auto_dial(nReference, dwParam, dwUser);
			break;
		case E_RCV_Ss7IsupCpg:
			//nReference   通道的逻辑编号
			//dwParam   CPG消息的长度。
			BOOST_LOG_SEV(cia_g_logger, AllEvent) << "业务流水:" << m_trunk_vector.at(nReference)->m_call_out_param->m_ch_msg->m_procbuffer_msg.transid()
				<< " 语音卡事件: E_RCV_Ss7IsupCpg, 通道的逻辑编号:" << nReference << ", CPG消息的长度" << dwParam;
			return 1;
			break;
		case E_CHG_ChState:
			//nReference 通道的逻辑编号
			//dwParam 高16位比特表示通道的上一个状态值；低16位比特表示通道的新状态值。有关通道状态的取值及其描述请参见函数SsmGetChState的说明
			BOOST_LOG_SEV(cia_g_logger, AllEvent) << "业务流水:" << m_trunk_vector.at(nReference)->m_call_out_param->m_ch_msg->m_procbuffer_msg.transid()
				<< " 语音卡事件: E_CHG_ChState 状态机：通道状态发生变化, 通道的逻辑编号:" << nReference
				<< ", 通道状态转换值:" << dwParam;
			return 1;
			break;
		case E_CHG_Mtp2Status:
			//nReference SS7信令链路号
			//dwParam SS7信令链路状态值，具体为：
			//1：业务中断 2：初始定位 3：已定位 / 准备好 4：已定位 / 未准备好 5：业务开通 6：处理机故障
			BOOST_LOG_SEV(cia_g_logger, AllEvent) << "语音卡事件: E_CHG_Mtp2Status SS7信令链路：信令链路的状态发生变化, SS7信令链路号: "
				<< nReference << ", SS7信令链路状态值: " << dwParam;
			return 1;
			break;
		case E_CHG_PcmLinkStatus:
			//nReference 数字中继线的逻辑编号
			//dwParam 输出数字中继线的状态值。详细内容请参见函数SsmGetPcmLinkStatus中pwPcmLinkStatus参数的说明
			BOOST_LOG_SEV(cia_g_logger, AllEvent) << "业务流水:" << m_trunk_vector.at(nReference)->m_call_out_param->m_ch_msg->m_procbuffer_msg.transid()
				<< " 语音卡事件: E_CHG_PcmLinkStatus 数字中继线的同步状态发生变化";
			return 1;
			break;
		}
	}
	catch (std::out_of_range)
	{
		BOOST_LOG_SEV(cia_g_logger, Warning) << "异常: 可能向量越界情况出现, 下标值: " << nReference;
		return 1;
	}

	std::ostringstream os;
	os << std::hex << wEvent;
	BOOST_LOG_SEV(cia_g_logger, Warning) << "接受到未处理语音卡事件:" << os.str();
	return 1;
}

int voice_card_control::deal_e_proc_auto_dial(int nReference, DWORD dwParam, DWORD dwUser)
{
	int channelID = nReference;
	if (m_trunk_vector.at(channelID)->m_call_out_param == nullptr)
	{
		BOOST_LOG_SEV(cia_g_logger, Critical) << "严重：触发deal_e_proc_auto_dial的通道没有关联呼叫请求对象, 通道号码" << channelID;
		return 1;
	}
	std::string trans_id_ = m_trunk_vector.at(nReference)->m_call_out_param->m_ch_msg->m_procbuffer_msg.transid();
	switch (dwParam)
	{
	case DIAL_STANDBY:	            // 通道空闲，没有执行AutoDial任务
		break;
	case DIAL_DIALING:
	{
		BOOST_LOG_SEV(cia_g_logger, Debug) << "业务流水:" << trans_id_
			<< " DIAL_DIALING 事件: 正在发送被叫号码. 外呼通道号:" << channelID;
		boost::shared_ptr<trunk> t = m_trunk_vector.at(nReference);
		boost::unique_lock<boost::mutex> unique_lock_(t->m_trunk_mutex);
		t->m_step = TRK_CALLOUT_DAIL;
		t->m_callTime.start();
		break;
	}
	case DIAL_ECHOTONE:	            // TUP/ISUP通道：表明驱动程序收到对端交换机的地址齐消息(ACM)
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << trans_id_
			<< " 在判断ACM后, 接收到 DIAL_ECHOTONE 事件, 挂机. 通道号码:" << channelID;
		BOOST_LOG_SEV(cia_g_logger, CalloutMsg) << "业务流水:" << trans_id_
			<< " 本次从呼叫到检测振铃, 共用时: " << m_trunk_vector.at(nReference)->elpased();
		{
			boost::shared_ptr<trunk> t = m_trunk_vector.at(nReference);
			if (t->m_step != TRK_CALLOUT_DAIL && t->m_step != TRK_WAIT_CONNECT)
			{
				BOOST_LOG_SEV(cia_g_logger, Critical) << "严重：触发振铃时通道不是TRK_CALLOUT_DAIL 或 TRK_WAIT_CONNECT状态, 通道的状态:"
					<< t->m_step << " 流水号 : " << trans_id_ << "通道号码:" << channelID;
				return 1;
			}
			if (m_use_strategy)
			{
				if (t->elpased() <= m_cti_warning_elapse)
				{
					BOOST_LOG_SEV(cia_g_logger, CalloutMsg) << "业务流水:" << trans_id_ << " 本次呼叫触发延迟策略";
					if (t->m_trunk_mutex.try_lock())
					{
						t->m_step = TRK_SLEEP;
						t->m_trunk_mutex.unlock();
						m_sleep_channel_queue.put(channelID);//放入需要做延迟挂机处理的队列中
					}
					else
					{
						BOOST_LOG_SEV(cia_g_logger, Critical) << "业务流水:" << trans_id_ << "将通道号码放入延迟队列前，尝试改变通道状态， 但试图锁定通道失败";
					}
					return 1;
				}
			}
			cti_hangUp(channelID, CIA_CALL_SUCCESS);
		}
		break;
	case DIAL_NO_DIALTONE:
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << trans_id_
			<< " 没有在线路上检测到拨号音，AutoDial 任务失败. 通道号码:" << channelID;
		cti_hangUp(channelID, CIA_CALL_FAIL);
		break;
	case DIAL_BUSYTONE:
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << trans_id_
			<< " DIAL_BUSYTONE 事件,被叫用户忙. 依然返回成功, 因为用户已经可以看到验证码, 通道号码:" << channelID;
		cti_hangUp(channelID, CIA_CALL_SUCCESS);
		break;
	case DIAL_ECHO_NOVOICE:	        // 拨号完成后，线路上先是出现了回铃音，然后保持静默。AutoDial任务终止。只适用于模拟中继线通道
	case DIAL_NOVOICE:	            // 拨号完成后，线路上没有检测到回铃音，一直保持静默。AutoDial任务终止。只适用于模拟中继线通道
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << trans_id_
			<< " 拨号完成, 线路保持沉默, 通道号码:" << channelID;
		break;
	case DIAL_VOICE:	            // 被叫用户摘机，AutoDial任务完成
	case DIAL_VOICEF1:	            // 被叫用户摘机（检测到F1频率的应答信号），AutoDial任务完成。只适用于模拟中继线通道
	case DIAL_VOICEF2:	            // 被叫用户摘机（检测到F2频率的应答信号），AutoDial任务完成。只适用于模拟中继线通道
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << trans_id_
			<< " E_PROC_AutoDial事件,板卡接通挂机, 通道号码:" << channelID;
		cti_hangUp(channelID, CIA_CALL_SUCCESS);
		break;
	case DIAL_NOANSWER:	            // 被叫用户在指定时间内没有摘机，AutoDial失败。不适用于IP卡SIP通道
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << trans_id_
			<< " DIAL_NOANSWER 事件,被叫用户在指定时间内没有摘机，AutoDial失败, 通道号码:" << channelID;
		cti_hangUp(channelID, CIA_CALL_FAIL);
		break;
	case DIAL_FAILURE:	            // AutoDial任务失败。失败原因可以通过函数SsmGetAutoDialFailureReason获得
	{
		int fail_reason = SsmGetAutoDialFailureReason(channelID);
		if (fail_reason == ATDL_NULL)
		{
			return 1;
		}
		BOOST_LOG_SEV(cia_g_logger, Critical) << "业务流水:" << trans_id_
			<< " SsmGetAutoDialFailureReason 函数返回错误代码: " << SsmGetAutoDialFailureReason(channelID)
			<< "DIAL_FAILURE 事件, AutoDial失败, 通道号码:" << channelID;
		cti_hangUp(channelID, CIA_CALL_FAIL);
	}
		break;
	case DIAL_INVALID_PHONUM:
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << trans_id_
			<< " 被叫号码无效, 通道号码:" << channelID;
		cti_hangUp(channelID, CIA_CALL_FAIL);
		break;
	case DIAL_SESSION_PROCEEDING:	// IP卡SIP通道收到18X消息(180除外)
	case DIAL_ISDN_PROGRESS:	    // ISDN通道收到对端交换机的PROGRESS消息。详细的消息内容可通过函数SsmISDNGetProgressMsg获得
		BOOST_LOG_SEV(cia_g_logger, Critical) << "业务流水:" << trans_id_
			<< " 触发其他类AutoDial事件, 请注意观察, 通道号码:" << channelID;
		break;
	}
	return 1;
}

int voice_card_control::deal_e_sys_bargein(int nReference, DWORD dwParam, DWORD dwUser)
{
	BOOST_LOG_SEV(cia_g_logger, Critical) << "天啊, 呼叫没成功挂断, 不但被接听了, 还检测到对方在说话!!!!!!!!!!";
	return 1;
}

int voice_card_control::deal_e_rcv_ss7msu(int nReference, DWORD dwParam, DWORD dwUser)
{
	PUCHAR ppucMsuBuf;                       // MSU消息信令单元
	int nSs7MsuLength = SsmGetSs7Msu(&ppucMsuBuf);
	std::ostringstream os;
	os << std::hex;
	for (int i = 0; i < nSs7MsuLength; i++)
	{
		os << (int)ppucMsuBuf[i] << " ";
	}
	BOOST_LOG_SEV(cia_g_logger, Ss7Msg) << "接受到的信令为:" << os.str();
	if (nSs7MsuLength < 12){	             // 需要解析长度大于12的报文
		BOOST_LOG_SEV(cia_g_logger, Ss7Msg) << "SS7 MSU 消息长度判断不合本程序逻辑, 不予解析, 长度: " << nSs7MsuLength;
		return 1;
	}
	BYTE* pMsuBuf = (BYTE *)ppucMsuBuf;
	// 对第十位做判断
	// 01 IAM , 06 ACM , 2C CPG , 0C REL , 10 RLC , 09 ANM 摘机
	switch (pMsuBuf[10])
	{
	case 0x06:	                             // 第十位为0x06, 代表ACM消息
		switch (pMsuBuf[11] & 0x03)          // 判断11位的最后2个字节
		{
		case 1:	                             // 01不计费
			BOOST_LOG_SEV(cia_g_logger, Warning) << "ACM消息判断为非计费, 注意, 注意";
			return 1;
			break;
		case 2:	                             // 10计费, 除了这两个以外都暂时不做判断处理
			BOOST_LOG_SEV(cia_g_logger, Ss7Msg) << "ACM消息判断为计费, 继续判断语音通道";
			return 1;
			break;
		default:
			break;
		}
	}
	//if (pMsuBuf[0] == 0x85){	//0位0x85代表是ISUP信令
	//	//-----------------------------------------------------------------------------------下方代码处理通道号
	//	int relayNum = pMsuBuf[7];	//中继No;
	//	int channelID = (relayNum - 1) * 30	// 每条中继30个通道, 编号依次递增
	//		+ ((pMsuBuf[8] & 0xE0) / 0x20) * 30	// 取高3位
	//		+ (pMsuBuf[8] & 0x1F);	//  30是... ...
	//	if ((pMsuBuf[8] & 0x1F) > 15){	//	取低5位, 大于15 减去2, 小于 减去 1
	//		channelID -= 2;
	//	}
	//	else{
	//		channelID -= 1;
	//	}
	//	if (channelID >= nChannelCount || channelID < 0){
	//		pLog->debug("解析SS7 MSU 消息为无效消息, 不予处理, 通道号码：" + to_string(channelID));
	//		return 1;
	//	}
	//	if (pATrkCh[channelID].Step != TRK_CALLOUT_DAIL){
	//		return 1;
	//	}
	//	pLog->debug("接收到Ss7Msu, 并且通道状态为 TRK_CALLOUT_DAIL, 通道号码:" + to_string(channelID));
	//	//-----------------------------------------------------------------------------------下方代码处理ISUP 信令部分
	//	switch (pMsuBuf[10])
	//	{
	//	case 0x06:	//ACM消息
	//		switch (pMsuBuf[11] & 0x03)
	//		{
	//		case 1:	//不计费
	//		{
	//					pLog->debug("ACM消息判断为不计费, 返回失败, 挂机, 通道号码:" + to_string(channelID));
	//					cti_hangUp(channelID, CIA_CALL_FAIL);
	//					return 1;
	//					break;
	//		}
	//		case 2:	//计费
	//			pLog->debug("ACM消息判断为计费, 继续判断语音通道, 通道号码:" + to_string(channelID));
	//			pATrkCh[channelID].m_qMutex.Lock();
	//			pATrkCh[channelID].Step = TRK_CHEK_BARGEIN;
	//			pATrkCh[channelID].m_qMutex.Unlock();
	//			return 1;
	//			break;
	//		default:
	//			break;
	//		}
	//	default:
	//		break;
	//	}
	//}
	return 1;
}

void voice_card_control::deal_hungup_strategy()
{
	int chID;
	while (true)
	{
		try
		{
			chID = m_sleep_channel_queue.take();
		}
		catch (std::out_of_range)
		{
			boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
			continue;
		}
		boost::shared_ptr<trunk> t = m_trunk_vector.at(chID);
		boost::unique_lock<boost::mutex> unique_lock_(t->m_trunk_mutex);
		if (t->m_step != TRK_SLEEP)
		{
			// 重复挂机， 可能因为电话接听等原因此次呼叫已经被挂断
			return;
		}
		std::string trans_id_ = t->m_call_out_param->m_ch_msg->m_procbuffer_msg.transid();
		int sleeping_elapse = m_cti_sleeping_elapse - t->elpased();
		if (sleeping_elapse > 0)
		{
			BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << trans_id_ << " 通道号:" << chID
				<< "触发延迟挂机条件, 开始休眠" << sleeping_elapse << "毫秒";
			boost::this_thread::sleep_for(boost::chrono::milliseconds(sleeping_elapse));
		}
		else
		{
			BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "业务流水:" << trans_id_ << " 通道号:" << chID
				<< "睡个毛, 去干活. 睡眠时间为:" << sleeping_elapse << "毫秒";
		}
		t->m_step = TRK_HUNGUP;
		unique_lock_.unlock();
		cti_hangUp(chID, CIA_CALL_SUCCESS);
	}
}

/**
 * \brief 获取语音卡空闲通道数
 *
 * \return 返回语音卡空闲通道数
 */
std::size_t voice_card_control::get_idol_channel_number()
{
	return m_channel_queue.size();
}

#endif	//CTI_VOICE_CARD_CONTROL_HPP_INCLUDED