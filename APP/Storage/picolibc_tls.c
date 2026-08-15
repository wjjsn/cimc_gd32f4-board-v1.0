#include <stdint.h>

/*
 * ARM 32 位 TLS ABI 在线程指针后保留 8 字节 TCB。picolibc 的 errno 是 TLS
 * 变量，编译器会以“线程指针 + 8 + 变量偏移”访问。当前固件是单线程裸机，
 * 因此直接让线程指针指向链接脚本分配的唯一 TLS 块之前 8 字节即可。
 */
extern uint8_t __tls_base[];

void *__aeabi_read_tp(void)
{
	return __tls_base - 8U;
}
