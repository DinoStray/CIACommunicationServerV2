#ifndef CHAT_MESSAGE_HPP
#define CHAT_MESSAGE_HPP

#include "../system/include_sys.h"
#include "CiaProtocol.pb.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

/**
 * \brief ��ͨѶ������ͨѶ�ķ�װ��
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
	 * \brief �������յ��ı���ͷ�� �����������Ľ����������ĳ��ȣ�
	 *
	 * \return true �����ɹ��� false ����ʧ�ܣ� �����峤�ȳ�����󻺳��� 
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
	 * \brief ���ñ������ ���ô˷����ɱ��������ñ���ͷ��ֵ
	 *
	 */
	void encode_header()
	{
		((int*)m_data)[0] = htonl(m_body_length);
	}

public:
	enum { header_length = 4 };                   // ���ݴ���ǰ4���ֽڣ���������ͷ�� �������汨���壨���ĸ��ֽ��Ժ󣩵ĳ���
	enum { max_body_length = 512 };               // ���������󳤶�
	ciaMessage m_procbuffer_msg;                  // ����ı��Ĳ���google��protocol buffer�����ݷ�װ
private:
	char m_data[header_length + max_body_length]; // ���Ļ�����
	std::size_t m_body_length;                    // �������ʵ�ʳ��ȣ� ���ڴ��䱨�ĵ�ǰ4���ֽ��б���
};

#endif // CHAT_MESSAGE_HPP