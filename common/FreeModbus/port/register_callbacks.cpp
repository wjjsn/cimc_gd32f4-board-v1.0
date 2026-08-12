#include "modbus_app.hpp"
#include "modbus_config.hpp"
#include "modbus_register_map.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

extern "C" {
#include "mb.h"
}

namespace
{
// Modbus一个寄存器只有16位，下面两个函数用于拆32位整数或float的原始位。
uint16_t high_word(uint32_t value)
{
	return static_cast<uint16_t>(value >> 16);
}

uint16_t low_word(uint32_t value)
{
	return static_cast<uint16_t>(value);
}

void store_u32(std::array<uint16_t, ModbusRegisterMap::Input::count> &registers,
	       uint16_t address, uint32_t value)
{
	// address是1-based回调地址，减去区域start后才是C++数组下标。
	const size_t index = address - ModbusRegisterMap::Input::start;
	if constexpr (ModbusConfig::word_order == ModbusWordOrder::high_word_first) {
		registers[index] = high_word(value);
		registers[index + 1] = low_word(value);
	} else {
		registers[index] = low_word(value);
		registers[index + 1] = high_word(value);
	}
}

template <size_t Size>
void store_float(std::array<uint16_t, Size> &registers, uint16_t start,
		 uint16_t address, float value)
{
	// 不做数值转换，直接保留IEEE754的32位原始位，再按配置拆成两个寄存器。
	uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	const size_t index = address - start;
	if constexpr (ModbusConfig::word_order == ModbusWordOrder::high_word_first) {
		registers[index] = high_word(bits);
		registers[index + 1] = low_word(bits);
	} else {
		registers[index] = low_word(bits);
		registers[index + 1] = high_word(bits);
	}
}

template <size_t Size>
float load_float(const std::array<uint16_t, Size> &registers, uint16_t start,
		 uint16_t address)
{
	// 这是store_float的反向过程，供保持寄存器写入时把两个16位字还原成float。
	const size_t index = address - start;
	uint32_t bits;
	if constexpr (ModbusConfig::word_order == ModbusWordOrder::high_word_first)
		bits = (static_cast<uint32_t>(registers[index]) << 16) |
		       registers[index + 1];
	else
		bits = (static_cast<uint32_t>(registers[index + 1]) << 16) |
		       registers[index];
	float value;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

template <size_t Size>
void write_registers(UCHAR *buffer, const std::array<uint16_t, Size> &registers,
		     uint16_t start, USHORT address, USHORT count)
{
	// Modbus规定单个16位寄存器在线上永远高字节先发。
	for (USHORT i = 0; i < count; ++i) {
		const uint16_t value = registers[address - start + i];
		*buffer++ = static_cast<UCHAR>(value >> 8);
		*buffer++ = static_cast<UCHAR>(value);
	}
}

bool range_valid(uint16_t start, uint16_t size, USHORT address, USHORT count)
{
	// FreeModbus传进来的address已经是1-based；范围不合法会返回异常码02。
	return count > 0U && address >= start &&
	       static_cast<uint32_t>(address) + count <=
		       static_cast<uint32_t>(start) + size;
}

bool get_packed_bit(const UCHAR *buffer, uint16_t index)
{
	// 线圈/离散输入按位打包，最低地址放在返回数据第一个字节的bit0。
	return (buffer[index / 8U] & (1U << (index % 8U))) != 0U;
}

void set_packed_bit(UCHAR *buffer, uint16_t index, bool value)
{
	const UCHAR mask = static_cast<UCHAR>(1U << (index % 8U));
	if (value)
		buffer[index / 8U] |= mask;
	else
		buffer[index / 8U] &= static_cast<UCHAR>(~mask);
}
}

extern "C" eMBErrorCode eMBRegInputCB(UCHAR *buffer, USHORT address,
				       USHORT count)
{
	/*
	 * FC04会进这里。流程很简单：取一次业务快照，构造完整输入寄存器数组，再把主站
	 * 请求的那一段复制出去。新增只读输入字段时，在下面的“构造数组”区域加一行。
	 */
	using namespace ModbusRegisterMap;
	if (!range_valid(Input::start, Input::count, address, count))
		return MB_ENOREG;

	ModbusAppSnapshot snapshot{};
	modbus_app_get_snapshot(snapshot);
	std::array<uint16_t, Input::count> registers{};
	// 测量值来自设备缓存，不在回调里直接做可能阻塞的ADC/I2C读取。
	store_float(registers, Input::start, Input::ch0, snapshot.ch0);
	store_float(registers, Input::start, Input::ch1, snapshot.ch1);
	store_float(registers, Input::start, Input::ch2, snapshot.ch2);
	store_u32(registers, Input::utc, snapshot.utc);
	// 固件版本2.0.1.0按四个字节打包：0x0200、0x0100。
	registers[Input::firmware - Input::start] = 0x0200;
	registers[Input::firmware - Input::start + 1] = 0x0100;
	// 状态寄存器按bit组合。增加状态位时同步更新README中的bit说明。
	registers[Input::status - Input::start] =
		(snapshot.auto_report ? 1U << 0 : 0U) |
		(snapshot.sleeping ? 1U << 1 : 0U) |
		(snapshot.ch0_alarm ? 1U << 2 : 0U) |
		(snapshot.ch1_alarm ? 1U << 3 : 0U);
	registers[Input::alarm_count - Input::start] = snapshot.alarm_count;
	registers[Input::mode - Input::start] =
		ModbusConfig::mode == ModbusSerialMode::ascii ? 1U : 0U;
	registers[Input::slave_address - Input::start] = ModbusConfig::slave_address;
	store_u32(registers, Input::baudrate, ModbusConfig::baudrate);
	write_registers(buffer, registers, Input::start, address, count);
	return MB_ENOERR;
}

extern "C" eMBErrorCode eMBRegHoldingCB(UCHAR *buffer, USHORT address,
					 USHORT count, eMBRegisterMode mode)
{
	/*
	 * FC03读、FC06/FC16写都会进这里。先用当前快照构造一份完整寄存器数组：
	 * 读请求直接返回数组片段；写请求先把主站数据覆盖到数组，再解析出被改动的业务字段。
	 * 这种做法让FC16一次写多个相邻字段时仍然能统一校验和提交。
	 */
	using namespace ModbusRegisterMap;
	if (!range_valid(Holding::start, Holding::count, address, count))
		return MB_ENOREG;

	ModbusAppSnapshot snapshot{};
	modbus_app_get_snapshot(snapshot);
	std::array<uint16_t, Holding::count> registers{};
	// 这一段是“保持寄存器读映射”。新增字段后，不要只写写入逻辑，读回也要在这里补。
	registers[Holding::device_id - Holding::start] = snapshot.device_id;
	registers[Holding::legacy_baudrate_code - Holding::start] =
		snapshot.legacy_baudrate_code;
	store_float(registers, Holding::start, Holding::ch0_ratio, snapshot.ch0_ratio);
	store_float(registers, Holding::start, Holding::ch1_ratio, snapshot.ch1_ratio);
	store_float(registers, Holding::start, Holding::ch0_threshold,
		    snapshot.ch0_threshold);
	store_float(registers, Holding::start, Holding::ch1_threshold,
		    snapshot.ch1_threshold);
	store_float(registers, Holding::start, Holding::ch2_threshold,
		    snapshot.ch2_threshold);
	registers[Holding::report_interval - Holding::start] = snapshot.report_interval;
	registers[Holding::alarm_mode - Holding::start] = snapshot.alarm_mode;
	registers[Holding::dac_raw - Holding::start] = snapshot.dac_raw;

	if (mode == MB_REG_READ) {
		write_registers(buffer, registers, Holding::start, address, count);
		return MB_ENOERR;
	}

	// 写请求buffer中每个寄存器仍是高字节在前，先覆盖到完整数组的对应位置。
	for (USHORT i = 0; i < count; ++i)
		registers[address - Holding::start + i] =
			(static_cast<uint16_t>(buffer[i * 2U]) << 8) | buffer[i * 2U + 1U];

	ModbusAppSnapshot updated = snapshot;
	uint32_t changes = 0;
	// touched表示请求碰到了字段；fully_touched用于判断两个寄存器是否完整覆盖。
	auto touched = [address, count](uint16_t reg, uint16_t width = 1) {
		return address < reg + width && reg < address + count;
	};
	auto fully_touched = [address, count](uint16_t reg, uint16_t width) {
		return address <= reg && address + count >= reg + width;
	};
	/*
	 * float必须整对写。FC06只能写一个寄存器，因此写float要用FC16；否则返回MB_EINVAL，
	 * 主站通常看到异常码03。新增可写float时一定要把地址加入这个列表。
	 */
	for (uint16_t float_address : {
		     Holding::ch0_ratio, Holding::ch1_ratio,
		     Holding::ch0_threshold, Holding::ch1_threshold,
		     Holding::ch2_threshold }) {
		if (touched(float_address, 2) && !fully_touched(float_address, 2))
			return MB_EINVAL;
	}
	// 从这里开始把寄存器解析回业务字段，并用changes记录本次真正触碰的字段。
	if (touched(Holding::device_id)) {
		updated.device_id = registers[Holding::device_id - Holding::start];
		changes |= MODBUS_CHANGE_DEVICE_ID;
	}
	if (touched(Holding::legacy_baudrate_code)) {
		updated.legacy_baudrate_code = static_cast<uint8_t>(
			registers[Holding::legacy_baudrate_code - Holding::start]);
		changes |= MODBUS_CHANGE_LEGACY_BAUDRATE;
	}
	if (touched(Holding::ch0_ratio, 2)) {
		updated.ch0_ratio = load_float(registers, Holding::start, Holding::ch0_ratio);
		changes |= MODBUS_CHANGE_CH0_RATIO;
	}
	if (touched(Holding::ch1_ratio, 2)) {
		updated.ch1_ratio = load_float(registers, Holding::start, Holding::ch1_ratio);
		changes |= MODBUS_CHANGE_CH1_RATIO;
	}
	if (touched(Holding::ch0_threshold, 2)) {
		updated.ch0_threshold =
			load_float(registers, Holding::start, Holding::ch0_threshold);
		changes |= MODBUS_CHANGE_CH0_THRESHOLD;
	}
	if (touched(Holding::ch1_threshold, 2)) {
		updated.ch1_threshold =
			load_float(registers, Holding::start, Holding::ch1_threshold);
		changes |= MODBUS_CHANGE_CH1_THRESHOLD;
	}
	if (touched(Holding::ch2_threshold, 2)) {
		updated.ch2_threshold =
			load_float(registers, Holding::start, Holding::ch2_threshold);
		changes |= MODBUS_CHANGE_CH2_THRESHOLD;
	}
	if (touched(Holding::report_interval)) {
		updated.report_interval = static_cast<uint8_t>(
			registers[Holding::report_interval - Holding::start]);
		changes |= MODBUS_CHANGE_REPORT_INTERVAL;
	}
	if (touched(Holding::alarm_mode)) {
		updated.alarm_mode = static_cast<uint8_t>(
			registers[Holding::alarm_mode - Holding::start]);
		changes |= MODBUS_CHANGE_ALARM_MODE;
	}
	if (touched(Holding::dac_raw)) {
		updated.dac_raw = registers[Holding::dac_raw - Holding::start];
		changes |= MODBUS_CHANGE_DAC;
	}

	// 业务层统一做合法范围、硬件动作和Flash保存；失败时返回异常码03。
	if (!modbus_app_apply_snapshot(updated, changes))
		return MB_EINVAL;
	return MB_ENOERR;
}

extern "C" eMBErrorCode eMBRegCoilsCB(UCHAR *buffer, USHORT address,
				       USHORT count, eMBRegisterMode mode)
{
	/*
	 * FC01读取、FC05/FC15写入会进这里。线圈不是16位寄存器，而是按bit打包。
	 * 新增线圈时要同时补读映射、写switch、业务setter和地址count。
	 */
	using namespace ModbusRegisterMap;
	if (!range_valid(Coil::start, Coil::count, address, count))
		return MB_ENOREG;

	ModbusAppSnapshot snapshot{};
	modbus_app_get_snapshot(snapshot);
	if (mode == MB_REG_READ) {
		// FreeModbus给出的buffer可能复用旧内容，打包前必须先清零。
		std::memset(buffer, 0, (count + 7U) / 8U);
		for (USHORT i = 0; i < count; ++i) {
			const uint16_t current = address + i;
			const bool value =
				current == Coil::auto_report ? snapshot.auto_report :
				current == Coil::alarm_report ? snapshot.alarm_mode == 1 :
				current == Coil::work_led ? snapshot.work_led : false;
			set_packed_bit(buffer, i, value);
		}
		return MB_ENOERR;
	}

	// 写多个线圈时，buffer的bit0对应本次请求的起始地址，而不是固定对应00001。
	for (USHORT i = 0; i < count; ++i) {
		const bool value = get_packed_bit(buffer, i);
		switch (address + i) {
		case Coil::auto_report: modbus_app_set_auto_report(value); break;
		case Coil::alarm_report: modbus_app_set_alarm_report(value); break;
		case Coil::work_led: modbus_app_set_work_led(value); break;
		default: return MB_ENOREG;
		}
	}
	return MB_ENOERR;
}

extern "C" eMBErrorCode eMBRegDiscreteCB(UCHAR *buffer, USHORT address,
					  USHORT count)
{
	/*
	 * FC02只读离散输入。处理方式和线圈读取一样，但没有写入分支。
	 * app_ready没有独立快照字段：匹配到它时表达式最后一项直接返回true。
	 */
	using namespace ModbusRegisterMap;
	if (!range_valid(Discrete::start, Discrete::count, address, count))
		return MB_ENOREG;

	ModbusAppSnapshot snapshot{};
	modbus_app_get_snapshot(snapshot);
	std::memset(buffer, 0, (count + 7U) / 8U);
	for (USHORT i = 0; i < count; ++i) {
		const uint16_t current = address + i;
		const bool value =
			current == Discrete::ch0_alarm ? snapshot.ch0_alarm :
			current == Discrete::ch1_alarm ? snapshot.ch1_alarm :
			current == Discrete::auto_report ? snapshot.auto_report :
			current == Discrete::sleeping ? snapshot.sleeping :
			current == Discrete::app_ready;
		set_packed_bit(buffer, i, value);
	}
	return MB_ENOERR;
}
