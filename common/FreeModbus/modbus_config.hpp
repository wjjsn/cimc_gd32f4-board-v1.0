#pragma once

#include <cstdint>

enum class ModbusSerialMode : uint8_t {
	rtu,
	ascii,
};

enum class ModbusSerialParity : uint8_t {
	none,
	odd,
	even,
};

enum class ModbusWordOrder : uint8_t {
	high_word_first, // ABCD：32位高16位放在低地址寄存器
	low_word_first, // CDAB：32位低16位放在低地址寄存器
};

namespace ModbusConfig
{
/*
 * 现场通信参数集中放在这里，避免到串口/DMA底层到处找。
 * 普通现场适配只改mode、slave_address、baudrate、parity和word_order。
 * 下面的中断优先级和timer_clock_hz已经实机验证，除非硬件时钟树改变，否则不要改。
 */
// RTU使用二进制帧和CRC；ASCII使用冒号、ASCII Hex、LRC和CRLF。
inline constexpr ModbusSerialMode mode = ModbusSerialMode::rtu;
// Modbus普通从站合法范围1-247；0是广播地址，不能配置成普通从站。
inline constexpr uint8_t slave_address = 1;
// FreeModbus的逻辑端口号。当前项目只有USART0这一个Modbus端口，保持0即可。
inline constexpr uint8_t serial_port = 0;
inline constexpr uint32_t baudrate = 19200;
// 当前默认RTU 8E1；ASCII模式下同一个配置对应7E1。
inline constexpr ModbusSerialParity parity = ModbusSerialParity::even;
// 只影响uint32/float的两个16位字顺序，不改变单个寄存器内部的高字节先发规则。
inline constexpr ModbusWordOrder word_order = ModbusWordOrder::high_word_first;

// USART0必须优先及时处理发送和IDLE；TIMER6稍低，用于RTU t3.5/ASCII字符超时。
inline constexpr uint8_t usart_irq_priority = 0;
inline constexpr uint8_t usart_irq_sub_priority = 0;
inline constexpr uint8_t timer_irq_priority = 1;
inline constexpr uint8_t timer_irq_sub_priority = 0;
// TIMER6实际输入时钟是120MHz，不是SystemCoreClock，也不是串口波特率。
inline constexpr uint32_t timer_clock_hz = 120000000U;
}
