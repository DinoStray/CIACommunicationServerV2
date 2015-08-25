#ifndef BOOST_LOG_HPP_INCLUDE_
#define BOOST_LOG_HPP_INCLUDE_
#include <boost/log/common.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/utility/setup/from_stream.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include "blocking_queue.hpp"

#include <exception>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>



namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;

/**
* 定义log日志级别， log记录使用 boost：：log 组件
*
* \author LYL QQ-331461049
* \date 2015/07/20 12:10
*/
enum severity_level
{
	AllEvent,    //开启所有事件输出
	Ss7Msg,	     // 开启ss7信令输出, 如非必要, 请开启语音卡服务器的信令日志输出, 没必要在应用日志输出
	CalloutMsg,	 // 开启每次呼出的日志记录
	Debug,	     // 开启调试信息
	RuntimeInfo, // 开启程序正常运行输出, 一般生产环境开启此日志
	Warning,	 // 警告, 一般为需要注意的日志信息, 有利于以后程序的调优
	Critical	 // 灾难, 遇到可能导致程序崩溃的异常
};
//  全局日志声明
BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(cia_lg, src::severity_logger< >)

src::severity_logger< >& cia_g_logger = cia_lg::get();	                //日志记录

struct LOG_MSG
{
	LOG_MSG(severity_level level_, std::string& msg){
		m_level = level_;
		m_msg = msg;
	}
	severity_level m_level;
	std::string m_msg;
};

blocking_queue<boost::shared_ptr<LOG_MSG>> LOG_MSG_QUEUE;
boost::thread_group log_thread;

void do_deal_log()
{
	while (true)
	{
		boost::this_thread::interruption_point();
		boost::shared_ptr<LOG_MSG> log_ = LOG_MSG_QUEUE.Take();
		BOOST_LOG_SEV(cia_g_logger, log_->m_level) << log_->m_msg;
	}
}

void stop_log()
{
	log_thread.interrupt_all();
}

void init_log(std::string log_config_file)
{
	try
	{
		std::ifstream settings(log_config_file);
		if (!settings.is_open())
		{
			throw std::runtime_error("无法打开配置文件:cia_log.config");
		}

		// Read the settings and initialize logging library
		logging::init_from_stream(settings);

		// Add some attributes
		logging::core::get()->add_global_attribute("TimeStamp", attrs::local_clock());
		BOOST_LOG_SEV(cia_g_logger, RuntimeInfo) << "日志组件初始化完毕";
	}
	catch (std::exception& e)
	{
		throw std::runtime_error(e.what());
	}
	log_thread.create_thread(do_deal_log);
}
#endif	// !BOOST_LOG_HPP_INCLUDE_