#pragma once

#include <cstdint>

/*
 * 这个快照是FreeModbus协议层和APP业务层之间的“中转箱”。
 *
 * 读寄存器：device.hpp把当前真实状态填进快照，回调再把它拆成16位寄存器。
 * 写寄存器：回调先修改快照并生成changes位图，device.hpp校验后才真正改参数/硬件。
 *
 * 新增业务字段时，通常先在这里加字段，再按OFFLINE_GUIDE.md接通两边。只搬现有地址
 * 不需要改这个结构。
 */
struct ModbusAppSnapshot {
	float ch0; // 30001-30002，只读测量值
	float ch1; // 30003-30004，只读测量值
	float ch2; // 30005-30006，只读测量值
	uint32_t utc; // 30007-30008，只读UTC秒
	uint16_t device_id; // 40001，旧协议设备ID
	uint8_t legacy_baudrate_code; // 40002，旧协议USART1波特率代码
	float ch0_ratio; // 40003-40004
	float ch1_ratio; // 40005-40006
	float ch0_threshold; // 40007-40008
	float ch1_threshold; // 40009-40010
	float ch2_threshold; // 40011-40012
	uint8_t report_interval; // 40013，1/2/3
	uint8_t alarm_mode; // 40014，1=主动，2=不主动
	uint16_t dac_raw; // 40015，立即作用于DAC
	uint16_t alarm_count; // 30012，只读
	bool ch0_alarm; // 10001以及30011 bit2
	bool ch1_alarm; // 10002以及30011 bit3
	bool auto_report; // 00001/10003以及30011 bit0
	bool sleeping; // 10004以及30011 bit1
	bool work_led; // 00003
};

/*
 * changes是写保持寄存器时的位图。它让业务层知道这次请求到底改了哪些字段，避免把
 * 快照中只是“顺便读出来”的其他值也重复写入。新增可写保持寄存器时必须分配一个新bit，
 * 并在register_callbacks.cpp和device.hpp两边使用同一个bit。
 */
enum ModbusAppChange : uint32_t {
	MODBUS_CHANGE_DEVICE_ID = 1U << 0,
	MODBUS_CHANGE_LEGACY_BAUDRATE = 1U << 1,
	MODBUS_CHANGE_CH0_RATIO = 1U << 2,
	MODBUS_CHANGE_CH1_RATIO = 1U << 3,
	MODBUS_CHANGE_CH0_THRESHOLD = 1U << 4,
	MODBUS_CHANGE_CH1_THRESHOLD = 1U << 5,
	MODBUS_CHANGE_CH2_THRESHOLD = 1U << 6,
	MODBUS_CHANGE_REPORT_INTERVAL = 1U << 7,
	MODBUS_CHANGE_ALARM_MODE = 1U << 8,
	MODBUS_CHANGE_DAC = 1U << 9,
};

// 读取当前设备状态。实现桥接在APP/Function/main.cpp，真实逻辑在Device类。
void modbus_app_get_snapshot(ModbusAppSnapshot &snapshot);
// 校验并应用保持寄存器写入。返回false时主站会收到非法数据值异常。
bool modbus_app_apply_snapshot(const ModbusAppSnapshot &snapshot,
			       uint32_t changes);
// 线圈写入是立即动作，使用单独接口，不经过保持寄存器changes位图。
void modbus_app_set_auto_report(bool enabled);
void modbus_app_set_alarm_report(bool enabled);
void modbus_app_set_work_led(bool enabled);
