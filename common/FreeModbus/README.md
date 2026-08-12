# FreeModbus 离线现场速查

这份文档按“现场断网、时间不够、拿到客户寄存器表后马上要改”的情况写。
先看下面的十步清单；如果客户不只是换地址，而是新增了字段、改变了数据类型，
再打开同目录的 `OFFLINE_GUIDE.md`，里面有可以直接照抄的模板。

`common/freemodbus/` 是未修改的 FreeModbus 1.6.0 上游子模块。项目自己的配置、
GD32F4 移植和业务寄存器映射都在 `common/FreeModbus/`。一般不要直接修改上游子模块。

## 现场十步清单

1. 先确认客户写的地址是 `30001/40001` 这种显示地址，还是报文里的 0-based 地址。
2. 确认每项数据属于输入寄存器、保持寄存器、线圈还是离散输入。
3. 确认数据类型：`uint16` 占1个寄存器，`uint32/float` 占2个连续寄存器。
4. 只调整现有字段地址时，优先只改 `modbus_register_map.hpp`。
5. 新增、删除或改变字段含义时，按 `OFFLINE_GUIDE.md` 同步修改快照和回调。
6. 检查同一区域内地址不能重叠，32位数据不能拆开。
7. 把该区域的 `count` 更新到能够覆盖最后一个地址。
8. 在 `APP` 目录编译，不要在仓库根目录直接跑 Meson。
9. 先运行测试脚本的 `--read-only`，确认读操作和异常响应。
10. 确认安全后再跑完整测试；脚本会把DAC和工作灯写入后恢复原值。

一句话判断修改范围：

- 只是把已有字段从一个地址搬到另一个地址：通常只改 `modbus_register_map.hpp`。
- 新增字段、改变类型、改变读写动作：还要改 `modbus_app.hpp`、
  `register_callbacks.cpp` 和 `APP/Function/device.hpp`。
- 改RTU/ASCII、从站地址、波特率、校验或32位字序：改 `modbus_config.hpp`。
- USART0、DMA1 Channel2、TIMER6属于已实机验证的底层，换寄存器表时不要动。

## 文件怎么分工

| 文件 | 你什么时候改 | 它负责什么 |
|---|---|---|
| `modbus_register_map.hpp` | 客户地址表变化时最先改 | 四类数据的地址、起始地址和数量 |
| `modbus_config.hpp` | 通信参数变化时改 | RTU/ASCII、地址、波特率、校验、32位字序 |
| `modbus_app.hpp` | 新增业务字段时改 | 协议层和APP业务层之间的数据快照 |
| `port/register_callbacks.cpp` | 新增字段或改变读写含义时改 | 把业务数据打包成Modbus寄存器，或把写入还原成业务数据 |
| `APP/Function/device.hpp` | 新增业务来源、校验或执行动作时改 | 读取真实设备状态、校验写入、保存Flash、驱动硬件 |
| `port/port_gd32f4.cpp` | 通常不要改 | USART0、DMA、IDLE、TIMER6和FreeModbus底层事件 |
| `tools/test_freemodbus.py` | 寄存器范围变化后同步改脚本顶部常量 | 离线串口回归测试 |

## 地址到底怎么算

这是现场最容易弄错的地方。项目里同时会看到三种写法：

| 含义 | 输入寄存器第1项 | 保持寄存器第1项 | 线圈第1项 | 离散输入第1项 |
|---|---:|---:|---:|---:|
| 人看的显示地址 | 30001 | 40001 | 00001 | 10001 |
| Modbus报文地址 | 0 | 0 | 0 | 0 |
| FreeModbus回调地址/本项目常量 | 1 | 1 | 1 | 1 |

例如主站要读 `40015`：

- 报文中的起始地址是 `14`，也就是十六进制 `0x000E`。
- `modbus_register_map.hpp` 中对应的回调地址是 `15`。
- Python测试脚本调用时也使用报文地址，所以写成 `address=14`。

换算公式：

```text
报文地址 = 本项目回调地址 - 1
30001显示地址 = 30000 + 本项目回调地址
40001显示地址 = 40000 + 本项目回调地址
00001/10001同理，最后五位就是本项目回调地址
```

注意：有些上位机界面让你填 `30001`，有些让你填 `0`。这不是设备协议变了，
只是上位机显示习惯不同。现场不确定时抓一帧报文最可靠。

## 四类数据和功能码

| 数据区 | 常见显示地址 | 权限 | 本项目回调 | 常用功能码 |
|---|---|---|---|---|
| 线圈 | 00001起 | 读写1位 | `eMBRegCoilsCB` | FC01读、FC05写单个、FC15写多个 |
| 离散输入 | 10001起 | 只读1位 | `eMBRegDiscreteCB` | FC02 |
| 输入寄存器 | 30001起 | 只读16位 | `eMBRegInputCB` | FC04 |
| 保持寄存器 | 40001起 | 读写16位 | `eMBRegHoldingCB` | FC03读、FC06写单个、FC16写多个 |

不要因为客户把一个只读量写在“40001表格”里就直接照搬。先问清楚它到底允许不允许写。
如果允许写并且写入要产生业务动作，就放保持寄存器；纯测量值通常放输入寄存器。

## 当前默认通信配置

配置集中在 `modbus_config.hpp`：

```cpp
inline constexpr ModbusSerialMode mode = ModbusSerialMode::rtu;
inline constexpr uint8_t slave_address = 1;
inline constexpr uint32_t baudrate = 19200;
inline constexpr ModbusSerialParity parity = ModbusSerialParity::even;
inline constexpr ModbusWordOrder word_order =
	ModbusWordOrder::high_word_first;
```

当前默认是：

- Modbus RTU
- 从站地址1
- 19200 bit/s
- 偶校验
- RTU串口格式8E1
- 32位数据高字在前，也就是常说的ABCD

切换标准Modbus ASCII只改：

```cpp
inline constexpr ModbusSerialMode mode = ModbusSerialMode::ascii;
```

ASCII使用7E1、冒号开头、CRLF结尾和LRC。它不是USART1上的
`A5B6...B6A5`旧ASCII Hex协议，两套协议互不替代。

从站地址只能是1到247。地址0是广播地址，不能拿来作为普通从站地址。

## 当前硬件资源

- FreeModbus串口：USART0
- TX：PA9
- RX：PA10
- RX DMA：DMA1 Channel2，Subperipheral4
- 帧间隔和ASCII字符超时：TIMER6
- USART0中断优先级高于TIMER6
- USART1继续运行原有 `A5B6/B6A5` 自定义协议

USART0接收采用“非循环DMA + IDLE中断 + 主循环DMA计数兜底”。这是为了解决连续字节
接收时USART0中断请求不稳定的问题，已经在真实GD32F470设备上验证。现场改寄存器表时，
不要把它改回逐字节RBNE中断。

当前按全双工UART使用。如果以后接半双工RS485，需要在
`common/Driver/hardware.hpp` 实现 `MODBUS_DIRECTION::init/receive/transmit`，
接入真实DE/RE引脚。发送结束切回接收前，端口层会等待USART TC，避免最后一字节丢失。

## 当前寄存器表

### 输入寄存器（FC04，只读）

| 显示地址 | 报文地址 | 内容 | 格式 |
|---|---:|---|---|
| 30001-30002 | 0-1 | CH0变比后采样 | IEEE754 float |
| 30003-30004 | 2-3 | CH1变比后采样 | IEEE754 float |
| 30005-30006 | 4-5 | CH2 PT100温度 | IEEE754 float |
| 30007-30008 | 6-7 | UTC时间戳 | uint32 |
| 30009-30010 | 8-9 | 固件版本2.0.1.0 | 4个uint8打包为2个寄存器 |
| 30011 | 10 | 状态位 | bit0自动上报，bit1睡眠，bit2/3为CH0/CH1告警 |
| 30012 | 11 | 告警记录数量 | uint16 |
| 30013 | 12 | 当前模式 | 0=RTU，1=ASCII |
| 30014 | 13 | 当前Modbus从站地址 | uint16 |
| 30015-30016 | 14-15 | 当前Modbus波特率 | uint32 |

### 保持寄存器（FC03/06/16，读写）

| 显示地址 | 报文地址 | 内容 | 合法值/说明 |
|---|---:|---|---|
| 40001 | 0 | 原协议设备ID | 1-65534 |
| 40002 | 1 | 原协议USART1波特率代码 | 0x11-0x14；注意不是Modbus USART0波特率 |
| 40003-40004 | 2-3 | CH0变比 | float，必须两个寄存器一起写 |
| 40005-40006 | 4-5 | CH1变比 | float，必须两个寄存器一起写 |
| 40007-40008 | 6-7 | CH0阈值 | float，必须两个寄存器一起写 |
| 40009-40010 | 8-9 | CH1阈值 | float，必须两个寄存器一起写 |
| 40011-40012 | 10-11 | CH2阈值 | float，必须两个寄存器一起写 |
| 40013 | 12 | 自动上报间隔代码 | 1=1秒，2=3秒，3=5秒 |
| 40014 | 13 | 告警模式 | 1=主动上报，2=不主动上报 |
| 40015 | 14 | DAC原始值 | 0-4095 |

除DAC外，上述参数写入成功后会保存到外部Flash。DAC会立即输出，但当前不会随这次写入
单独保存为持久参数。非法ID、非法代码、NaN、Inf、超范围DAC或只写float一半都会返回异常。

### 线圈（FC01/05/15，读写）

| 显示地址 | 报文地址 | 内容 |
|---|---:|---|
| 00001 | 0 | 启动/停止自动上报 |
| 00002 | 1 | 启用/禁用主动告警，并保存参数 |
| 00003 | 2 | 工作指示灯 |

### 离散输入（FC02，只读）

| 显示地址 | 报文地址 | 内容 |
|---|---:|---|
| 10001 | 0 | CH0告警状态 |
| 10002 | 1 | CH1告警状态 |
| 10003 | 2 | 自动上报状态 |
| 10004 | 3 | 睡眠状态 |
| 10005 | 4 | APP已就绪，恒为1 |

## 32位数据和字序

一个Modbus寄存器只有16位，所以 `uint32_t` 和 `float` 都要占两个连续寄存器。
每个16位寄存器内部固定是高字节先发，不能配置；可配置的是两个16位字谁在前。

假设32位原始值是 `0x11223344`：

```text
high_word_first（ABCD）：第一个寄存器=0x1122，第二个=0x3344
low_word_first （CDAB）：第一个寄存器=0x3344，第二个=0x1122
```

上位机float数值明显离谱但通信正常时，先检查 `word_order`，不要先改测量公式。

写float时必须使用FC16一次写两个寄存器。FC06只能写一个寄存器，因此不能拿来写本项目的float。

## 编译

两个Meson工程彼此独立。FreeModbus在APP中，改完后至少编译APP：

```sh
cd APP
PATH=/usr/bin:/bin:/usr/sbin:/sbin meson setup build --cross-file arm-none-eabi.ini
PATH=/usr/bin:/bin:/usr/sbin:/sbin meson compile -C build/
```

如果 `build/` 已经存在，不要重复setup，直接compile：

```sh
cd APP
PATH=/usr/bin:/bin:/usr/sbin:/sbin meson compile -C build/
```

做完整回归时再编译BootLoader：

```sh
cd BootLoader
PATH=/usr/bin:/bin:/usr/sbin:/sbin meson compile -C build/
```

## 离线串口测试

测试脚本已经放进仓库：`common/FreeModbus/tools/test_freemodbus.py`。
脚本使用PEP 723声明 `pyserial` 依赖，Windows上有 `uv.exe` 时直接运行。
下面命令都从仓库根目录执行；如果当前在 `APP` 目录，路径要改成
`..\common\FreeModbus\tools\test_freemodbus.py`。

所谓“离线可用”有一个前提：出发前在这台电脑上至少成功运行一次下面的 `uv.exe run`
命令，让uv把Python和 `pyserial` 放进本机缓存。全新电脑第一次运行时如果缓存里没有依赖，
uv仍可能需要联网下载。最稳妥的出发前检查就是拔掉网络后再跑一次 `--help`：

```powershell
uv.exe run common\FreeModbus\tools\test_freemodbus.py --help
```

先做只读测试，最安全：

```powershell
uv.exe run common\FreeModbus\tools\test_freemodbus.py --port COM31 --read-only
```

确认设备允许临时改变DAC和工作灯后，再做完整测试。脚本会读回原值并恢复：

```powershell
uv.exe run common\FreeModbus\tools\test_freemodbus.py --port COM31
```

ASCII模式：

```powershell
uv.exe run common\FreeModbus\tools\test_freemodbus.py --port COM31 --mode ascii
```

连续50次读测试，每帧之间保留4ms合法RTU间隔：

```powershell
uv.exe run common\FreeModbus\tools\test_freemodbus.py --port COM31 --repeat 50 --frame-gap-ms 4 --read-only
```

如果客户表改变了寄存器数量，先修改脚本顶部 `REGISTER_LAYOUT`，否则脚本会继续按旧范围读。

## 常见异常和最快处理办法

| 现象 | 最常见原因 | 先做什么 |
|---|---|---|
| 完全无响应 | TX/RX接反、未共地、串口格式不一致、从站地址不对 | 确认CH340 TX接PA10、RX接PA9、GND共地；核对19200 8E1 |
| 返回功能码加0x80、异常码02 | 地址超出 `start/count`，或表地址换算错 | 检查显示地址、报文地址、回调地址是否差1 |
| 返回异常码03 | 写入值非法，或只写了float的一半 | 对照合法范围，float改用FC16整对写 |
| 读到的float特别大或特别小 | 两个16位字顺序不一致 | 切换 `ModbusConfig::word_order` |
| uint16正常，ASCII数字偶尔大于0x7F | 7E1的校验位被当数据位 | 当前端口层已做 `& 0x7F`，不要删除该处理 |
| CRC/LRC错误帧没有响应 | 正常行为 | Modbus要求静默丢弃校验错误帧 |
| 改40002后Modbus波特率没变 | 40002控制旧协议USART1 | Modbus波特率在 `modbus_config.hpp` |
| 写参数后重启丢失 | 新字段没有加入 `changes` 或业务层没保存 | 检查 `ModbusAppChange`、`modbus_apply_snapshot()` 和 `params_save()` |
| 新增地址总返回02 | 忘记增大区域 `count` | `count` 必须覆盖最后一个寄存器 |
| 第一帧正常，快速连发失败 | 主站未留RTU帧间隔 | 19200下至少留当前配置的t3.5，测试建议4ms |

## 协议帧例子

默认地址1，读取输入寄存器30001-30002：

```text
RTU请求：   01 04 00 00 00 02 71 CB
ASCII请求： :010400000002F9\r\n
```

默认地址1，读取保持寄存器40015：

```text
PDU字段：功能码03，起始地址000E，数量0001
RTU无CRC部分：01 03 00 0E 00 01
```

## 睡眠和恢复

APP进入深度睡眠前调用 `modbus_slave_suspend()`。系统时钟恢复后调用
`modbus_slave_resume()`，它会重新初始化USART0、DMA、TIMER6和FreeModbus状态机。

## 已验证状态

真实GD32F470设备上已经验证：

- RTU和ASCII两种模式
- FC01、FC02、FC03、FC04、FC05、FC06
- 保持寄存器写入后读回并恢复
- 非法地址异常 `84 02`
- 错误CRC/LRC静默丢弃
- RTU 4ms帧间隔下连续50次FC04
- 完整APP调度器同时运行

最终交付固件已恢复为RTU、地址1、19200、偶校验、高字在前。
