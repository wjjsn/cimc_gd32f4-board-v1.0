# FreeModbus 断网改表手册

这份手册不要求现场能查网页。下面每个场景都按“改哪些文件、代码放哪里、最后检查什么”
来写。开始前先看 `README.md` 的地址换算和现场十步清单。

## 先记住项目的数据流

读寄存器时，数据按下面方向走：

```text
Device真实状态
  -> modbus_app_get_snapshot()
  -> ModbusAppSnapshot
  -> register_callbacks.cpp打包
  -> FreeModbus
  -> USART0响应
```

写保持寄存器时，方向反过来：

```text
USART0请求
  -> FreeModbus
  -> register_callbacks.cpp解包并标记changes
  -> modbus_app_apply_snapshot()
  -> 合法性检查
  -> 更新参数/硬件/Flash
```

所以只搬地址时不用碰业务层；新增字段时，地址、快照、打包/解包、业务来源或执行动作必须连起来。

## 场景一：只把现有字段换地址

例如客户要求把DAC从40015搬到40020。

只改 `modbus_register_map.hpp`：

```cpp
inline constexpr uint16_t dac_raw = 20;
inline constexpr uint16_t count = 20;
```

这里的20对应显示地址40020、报文地址19。`count`也要从15改成20，否则范围检查仍只允许
40001到40015，访问40020会返回异常码02。

检查：

- 40016到40019会成为空洞。目前回调数组会给空洞读出0，也允许范围内写入但没有业务效果。
- 如果客户不允许空洞，最好重新连续排表；不要只依赖 `count` 表示稀疏地址。
- 确认新地址没有和现有字段重叠。
- 修改测试脚本顶部布局后编译和只读测试。

## 场景二：新增一个只读uint16输入寄存器

例子：新增板卡温度 `board_temperature_tenth`，单位0.1摄氏度，放在30017。

第一步，在 `modbus_register_map.hpp` 增加地址并扩大数量：

```cpp
namespace Input
{
inline constexpr uint16_t board_temperature = 17;
inline constexpr uint16_t count = 17;
}
```

第二步，在 `modbus_app.hpp` 的快照中增加业务字段：

```cpp
uint16_t board_temperature_tenth;
```

第三步，在 `APP/Function/device.hpp` 的 `modbus_get_snapshot()` 填入真实数据：

```cpp
snapshot.board_temperature_tenth = board_temperature_tenth_;
```

第四步，在 `eMBRegInputCB()` 构建输入寄存器数组时写入：

```cpp
registers[Input::board_temperature - Input::start] =
	snapshot.board_temperature_tenth;
```

这个字段只读，所以不需要change位，也不需要 `modbus_app_apply_snapshot()`。

## 场景三：新增只读uint32

例子：新增累计运行秒数，占30017-30018。

地址表：

```cpp
inline constexpr uint16_t running_seconds = 17; // uint32，占17和18
inline constexpr uint16_t count = 18;
```

快照：

```cpp
uint32_t running_seconds;
```

业务层填值：

```cpp
snapshot.running_seconds = running_seconds_;
```

输入回调打包：

```cpp
store_u32(registers, Input::running_seconds, snapshot.running_seconds);
```

`store_u32()` 会自动服从 `ModbusConfig::word_order`。不要自己手写移位后又调用它，否则会换两次字序。

## 场景四：新增只读float

例子：新增电源电压，放在30017-30018。

地址表：

```cpp
inline constexpr uint16_t supply_voltage = 17; // float，占17和18
inline constexpr uint16_t count = 18;
```

快照：

```cpp
float supply_voltage;
```

业务层：

```cpp
snapshot.supply_voltage = supply_voltage_;
```

输入回调：

```cpp
store_float(registers, Input::start, Input::supply_voltage,
	    snapshot.supply_voltage);
```

上位机必须一次读两个寄存器，并按IEEE754单精度解析。读一个寄存器不可能得到完整float。

## 场景五：新增可写uint16保持寄存器

例子：新增采样周期 `sample_period_ms`，放在40016，合法范围10到10000，并且要保存Flash。

第一步，地址表：

```cpp
inline constexpr uint16_t sample_period_ms = 16;
inline constexpr uint16_t count = 16;
```

第二步，快照：

```cpp
uint16_t sample_period_ms;
```

第三步，增加change位。每个值必须使用没有占用的新bit：

```cpp
MODBUS_CHANGE_SAMPLE_PERIOD = 1U << 10,
```

第四步，在保持寄存器回调的数组初始化部分加入读映射：

```cpp
registers[Holding::sample_period_ms - Holding::start] =
	snapshot.sample_period_ms;
```

第五步，在回调写入解析部分加入：

```cpp
if (touched(Holding::sample_period_ms)) {
	updated.sample_period_ms =
		registers[Holding::sample_period_ms - Holding::start];
	changes |= MODBUS_CHANGE_SAMPLE_PERIOD;
}
```

第六步，在 `device.hpp::modbus_get_snapshot()` 填当前值。

第七步，在 `device.hpp::modbus_apply_snapshot()` 先校验，再应用：

```cpp
if ((changes & MODBUS_CHANGE_SAMPLE_PERIOD) &&
    (snapshot.sample_period_ms < 10U || snapshot.sample_period_ms > 10000U))
	return false;

if (changes & MODBUS_CHANGE_SAMPLE_PERIOD)
	params_.sample_period_ms = snapshot.sample_period_ms;
```

如果它属于 `params_` 并且应持久化，现有 `if (changes & ~MODBUS_CHANGE_DAC) params_save();`
会保存它。前提是你也把字段加入 `DeviceParams`，并确认Flash结构升级策略。已经出货的设备修改
持久化结构时，不能只追加字段后假设旧Flash自动兼容。

## 场景六：新增可写float保持寄存器

例子：新增校准偏移 `calibration_offset`，放40016-40017。

地址表：

```cpp
inline constexpr uint16_t calibration_offset = 16; // float，占16和17
inline constexpr uint16_t count = 17;
```

快照和change位按上一个场景增加。在保持寄存器读映射里：

```cpp
store_float(registers, Holding::start, Holding::calibration_offset,
	    snapshot.calibration_offset);
```

必须把它加入“禁止只写一半”的列表：

```cpp
for (uint16_t float_address : {
	Holding::ch0_ratio,
	Holding::ch1_ratio,
	Holding::ch0_threshold,
	Holding::ch1_threshold,
	Holding::ch2_threshold,
	Holding::calibration_offset,
}) {
	if (touched(float_address, 2) && !fully_touched(float_address, 2))
		return MB_EINVAL;
}
```

解析：

```cpp
if (touched(Holding::calibration_offset, 2)) {
	updated.calibration_offset = load_float(
		registers, Holding::start, Holding::calibration_offset);
	changes |= MODBUS_CHANGE_CALIBRATION_OFFSET;
}
```

业务层至少检查 `std::isfinite()`，如果有实际上下限也一起检查。主站写入必须使用FC16，
起始报文地址是15，数量是2。

## 场景七：新增线圈

例子：新增蜂鸣器开关，放00004。

地址表：

```cpp
inline constexpr uint16_t buzzer = 4;
inline constexpr uint16_t count = 4;
```

快照增加：

```cpp
bool buzzer;
```

`modbus_app.hpp` 增加接口：

```cpp
void modbus_app_set_buzzer(bool enabled);
```

在 `APP/Function/main.cpp` 做全局桥接，在 `device.hpp` 实现真正硬件动作。

线圈读取映射的三元表达式会越来越难读。新增到第四个以上时，建议把当前地址判断改为switch：

```cpp
bool value = false;
switch (current) {
case Coil::auto_report: value = snapshot.auto_report; break;
case Coil::alarm_report: value = snapshot.alarm_mode == 1; break;
case Coil::work_led: value = snapshot.work_led; break;
case Coil::buzzer: value = snapshot.buzzer; break;
default: return MB_ENOREG;
}
```

写映射增加：

```cpp
case Coil::buzzer: modbus_app_set_buzzer(value); break;
```

## 场景八：新增离散输入

例子：新增门磁状态，放10006。

地址表增加 `door_open = 6`，并把 `count` 改为6；快照增加 `bool door_open`；
业务层填当前状态；最后在 `eMBRegDiscreteCB()` 地址判断中加入它。

离散输入没有写回调，不需要setter或change位。

## 场景九：增加状态寄存器中的bit

如果只是给30011增加一个状态位，不一定要新增寄存器。

例如bit4表示Flash故障：

```cpp
registers[Input::status - Input::start] =
	(snapshot.auto_report ? 1U << 0 : 0U) |
	(snapshot.sleeping ? 1U << 1 : 0U) |
	(snapshot.ch0_alarm ? 1U << 2 : 0U) |
	(snapshot.ch1_alarm ? 1U << 3 : 0U) |
	(snapshot.flash_error ? 1U << 4 : 0U);
```

同步更新README寄存器表。bit编号从0开始，bit4的掩码是 `0x0010`。

## 场景十：删除字段

删除不能只删地址常量。按数据流反向检查：

1. 删除回调中的打包或解包。
2. 删除不再使用的change位。
3. 删除快照字段。
4. 删除业务桥接和硬件动作。
5. 最后删除地址常量并缩小 `count`。
6. 搜索旧字段名，确认没有残留。
7. 更新测试脚本和文档。

如果地址中间留下空洞，要明确客户是否接受读0。严格协议表通常不建议留下可访问空洞。

## 修改通信参数

只改 `modbus_config.hpp` 的现场参数：

```cpp
inline constexpr ModbusSerialMode mode = ModbusSerialMode::rtu;
inline constexpr uint8_t slave_address = 1;
inline constexpr uint32_t baudrate = 19200;
inline constexpr ModbusSerialParity parity = ModbusSerialParity::even;
inline constexpr ModbusWordOrder word_order = ModbusWordOrder::high_word_first;
```

不要为了改波特率去改 `timer_clock_hz`。它是TIMER6外设时钟120MHz，不是串口波特率。
也不要随便改中断优先级；当前优先级已经过实机验证。

## 写入失败时异常怎么来的

- 回调返回 `MB_ENOREG`：通常变成非法数据地址异常码02。
- 回调返回 `MB_EINVAL`：通常变成非法数据值异常码03。
- CRC或LRC错误：协议栈直接静默丢弃，不返回异常帧。
- 地址正确但业务层 `modbus_app_apply_snapshot()` 返回false：回调返回 `MB_EINVAL`。

所以看到异常码02先查地址和 `count`，看到03先查值范围、float是否整对写、业务校验。

## 编译前一分钟人工检查

- 四个区域的地址是否各自独立计算，没有把40001直接写成40001常量。
- `uint32/float` 是否占两个连续地址。
- 地址是否重叠。
- `count` 是否覆盖最后一个地址。
- 新只读字段是否已经在 `modbus_get_snapshot()` 填值。
- 新保持寄存器是否同时有读映射和写解析。
- 新可写字段是否有change位、合法性检查和应用动作。
- 新float是否加入禁止半写列表。
- 需要掉电保存的字段是否真正进入持久参数结构。
- README和测试脚本布局是否同步。

## 最短验证流程

```sh
cd APP
PATH=/usr/bin:/bin:/usr/sbin:/sbin meson compile -C build/
```

下载后先跑：

```powershell
uv.exe run common\FreeModbus\tools\test_freemodbus.py --port COM31 --read-only
```

这个命令按“当前目录是仓库根目录”书写。如果当前在 `APP`，脚本路径改成
`..\common\FreeModbus\tools\test_freemodbus.py`。出发前要联网成功运行至少一次，让uv缓存
`pyserial`；然后断网执行一次 `--help`，确认现场不依赖临时下载。

只读通过后，如果允许临时改变输出：

```powershell
uv.exe run common\FreeModbus\tools\test_freemodbus.py --port COM31
```

如果没有时间逐项排查，至少保证：编译通过、脚本只读通过、客户实际使用的一个读功能码和一个
写功能码通过。不要在完全没读回确认的情况下直接批量写客户参数。
