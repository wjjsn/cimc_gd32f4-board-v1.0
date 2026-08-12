#pragma once

#include <cstdint>

namespace ModbusRegisterMap
{
/*
 * 这是现场换寄存器表时最先改的文件。
 *
 * 这里统一写FreeModbus回调收到的1-based地址，不要直接写30001或40001：
 *   这里的1  <=> 输入寄存器30001/保持寄存器40001/线圈00001/离散输入10001
 *   报文地址 <=> 这里的地址减1
 *
 * count表示从start开始连续开放多少个地址，必须覆盖最后一个寄存器。例如最后一个
 * uint32从15开始，占15和16，那么count至少是16。地址不要重叠；float和uint32必须
 * 占两个连续寄存器。新增字段的完整步骤见OFFLINE_GUIDE.md。
 */
namespace Input
{
// FC04只读输入寄存器。显示地址 = 30000 + 下面的常量。
inline constexpr uint16_t start = 1;
inline constexpr uint16_t ch0 = 1; // 30001-30002，float，占2个寄存器
inline constexpr uint16_t ch1 = 3; // 30003-30004，float，占2个寄存器
inline constexpr uint16_t ch2 = 5; // 30005-30006，float，占2个寄存器
inline constexpr uint16_t utc = 7; // 30007-30008，uint32，占2个寄存器
inline constexpr uint16_t firmware = 9; // 30009-30010，4个版本字节
inline constexpr uint16_t status = 11; // 30011，bit状态集合
inline constexpr uint16_t alarm_count = 12; // 30012
inline constexpr uint16_t mode = 13; // 30013，0=RTU，1=ASCII
inline constexpr uint16_t slave_address = 14; // 30014
inline constexpr uint16_t baudrate = 15; // 30015-30016，uint32
inline constexpr uint16_t count = 16; // 当前开放30001-30016
}

namespace Holding
{
// FC03读、FC06/FC16写。显示地址 = 40000 + 下面的常量。
inline constexpr uint16_t start = 1;
inline constexpr uint16_t device_id = 1; // 40001，旧协议设备ID
inline constexpr uint16_t legacy_baudrate_code = 2; // 40002，控制USART1，不是USART0
inline constexpr uint16_t ch0_ratio = 3; // 40003-40004，float
inline constexpr uint16_t ch1_ratio = 5; // 40005-40006，float
inline constexpr uint16_t ch0_threshold = 7; // 40007-40008，float
inline constexpr uint16_t ch1_threshold = 9; // 40009-40010，float
inline constexpr uint16_t ch2_threshold = 11; // 40011-40012，float
inline constexpr uint16_t report_interval = 13; // 40013，1/2/3
inline constexpr uint16_t alarm_mode = 14; // 40014，1=主动，2=不主动
inline constexpr uint16_t dac_raw = 15; // 40015，0-4095
inline constexpr uint16_t count = 15; // 当前开放40001-40015
}

namespace Coil
{
// FC01读、FC05/FC15写。显示地址00001就是这里的1。
inline constexpr uint16_t start = 1;
inline constexpr uint16_t auto_report = 1; // 00001
inline constexpr uint16_t alarm_report = 2; // 00002
inline constexpr uint16_t work_led = 3; // 00003
inline constexpr uint16_t count = 3; // 当前开放00001-00003
}

namespace Discrete
{
// FC02只读离散输入。显示地址10001就是这里的1。
inline constexpr uint16_t start = 1;
inline constexpr uint16_t ch0_alarm = 1; // 10001
inline constexpr uint16_t ch1_alarm = 2; // 10002
inline constexpr uint16_t auto_report = 3; // 10003
inline constexpr uint16_t sleeping = 4; // 10004
inline constexpr uint16_t app_ready = 5; // 10005，恒为1
inline constexpr uint16_t count = 5; // 当前开放10001-10005
}
}
