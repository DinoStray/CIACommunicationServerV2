#ifndef CIA_DEF_H_INCLUDE_
#define CIA_DEF_H_INCLUDE_

#include <string>

const std::string CIA_LOGIN_REQUEST("000101");    // ��½����
const std::string CIA_LOGIN_RESPONSE("000301");   // ��½��Ӧ
const std::string CIA_LOGIN_SUCCESS("99");        // ��½�ɹ�
const std::string CIA_LOGIN_FAIL("98");           // ��½ʧ��
const std::string CIA_HEART_REQUEST("000302");    // ��������
const std::string CIA_HEART_RESPONSE("000102");   // ������Ӧ
const std::string CIA_CALL_REQUEST("010103");     // ��������
const std::string CIA_CALL_RESPONSE("010303");    // ���л�Ӧ
const std::string CIA_CALL_SUCCESS("99");         // ���гɹ�
const std::string CIA_CALL_FAIL("98");            // ����ʧ��
const std::string CIA_CALL_TIMEOUT("01");         // ��������ʱ

#endif // !CIA_DEF_H_INCLUDE_