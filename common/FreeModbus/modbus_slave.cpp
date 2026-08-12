#include "modbus_config.hpp"
#include "modbus_slave.hpp"

extern "C" {
#include "mb.h"

void vMBPortPoll(void);
}

namespace
{
// 把项目自己的枚举转换成FreeModbus 1.6.0使用的C枚举。
eMBParity parity()
{
	switch (ModbusConfig::parity) {
	case ModbusSerialParity::odd: return MB_PAR_ODD;
	case ModbusSerialParity::even: return MB_PAR_EVEN;
	case ModbusSerialParity::none: return MB_PAR_NONE;
	}
	return MB_PAR_NONE;
}

eMBMode mode()
{
	return ModbusConfig::mode == ModbusSerialMode::ascii ? MB_ASCII : MB_RTU;
}
}

bool modbus_slave_init()
{
	// 0是广播地址，248-255保留；作为普通从站只能使用1-247。
	if (ModbusConfig::slave_address == 0U || ModbusConfig::slave_address > 247U)
		return false;
	return eMBInit(mode(), ModbusConfig::slave_address,
		       ModbusConfig::serial_port, ModbusConfig::baudrate,
		       parity()) == MB_ENOERR &&
	       eMBEnable() == MB_ENOERR;
}

void modbus_slave_poll()
{
	/*
	 * 先把DMA/中断积累的字节和超时事件重放给协议FSM，再让eMBPoll处理完整帧和功能码。
	 * main.cpp每轮while都调用本函数，这是连续收帧稳定性的必要条件。
	 */
	vMBPortPoll();
	(void)eMBPoll();
}

void modbus_slave_suspend()
{
	(void)eMBDisable();
}

bool modbus_slave_resume()
{
	// 睡眠恢复后外设寄存器和协议状态都可能失效，完整关闭再初始化最稳妥。
	(void)eMBDisable();
	(void)eMBClose();
	return modbus_slave_init();
}
