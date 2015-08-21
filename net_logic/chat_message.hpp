#ifndef CHAT_MESSAGE_HPP
#define CHAT_MESSAGE_HPP

#include "../system/include_sys.h"
#include "CiaProtocol.pb.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

/**
 * \brief 与通讯端数据通讯的封装类
 *
 * \author LYL QQ-331461049
 * \date 2015/08/18 20:27
 */
class chat_message
{
public:
	chat_message():m_body_length(0){}

	chat_message(ciaMessage& msg)
	{
		body_length(msg.ByteSize());
		msg.SerializeToArray(body(), msg.ByteSize());
		encode_header();
		m_procbuffer_msg = msg;
	}

	bool parse_to_cia_msg()
	{
		body_length(m_procbuffer_msg.ByteSize());
		if (!m_procbuffer_msg.SerializeToArray(body(), m_procbuffer_msg.ByteSize()))
		{
			return false;
		}
		encode_header();
		return true;
	}
	bool parse_cia_mag()
	{
		return m_procbuffer_msg.ParseFromArray(body(), body_length());
	}

	const char* data() const
	{
		return m_data;
	}

	char* data()
	{
		return m_data;
	}

	std::size_t length() const
	{
		return header_length + m_body_length;
	}

	const char* body() const
	{
		return m_data + header_length;
	}

	char* body()
	{
		return m_data + header_length;
	}

	std::size_t body_length() const
	{
		return m_body_length;
	}

	void body_length(std::size_t new_length)
	{
		m_body_length = new_length;
		if (m_body_length > max_body_length)
			m_body_length = max_body_length;
	}

	/**
	 * \brief 解析接收到的报文头， 并保存解析后的结果（报文体的长度）
	 *
	 * \return true 解析成功， false 解析失败， 报文体长度超过最大缓冲区 
	 */
	bool decode_header()
	{
		m_body_length = ntohl(((int*)m_data)[0]);
		if (m_body_length > max_body_length)
		{
			m_body_length = 0;
			return false;
		}
		return true;
	}

	/**
	 * \brief 填充好报文体后， 调用此方法由报文体设置报文头的值
	 *
	 */
	void encode_header()
	{
		((int*)m_data)[0] = htonl(m_body_length);
	}

public:
	enum { header_length = 4 };                   // 数据传输前4个字节，用作报文头， 用来保存报文体（第四个字节以后）的长度
	enum { max_body_length = 512 };               // 报文体的最大长度
	ciaMessage m_procbuffer_msg;                  // 传输的报文采用google的protocol buffer做数据封装
private:
	char m_data[header_length + max_body_length]; // 报文缓冲区
	std::size_t m_body_length;                    // 报文体的实际长度， 放在传输报文的前4个字节中保存
};

#endif // CHAT_MESSAGE_HPP