# common/ 共享目录 — 合并决策文档

> 本文件记录 APP 与 BootLoader 两个工程共有内容的合并情况。
> **原则**：内容完全一致或已决策统一的内容移动到根目录 `common/`，并通过符号链接保持两工程目录结构；**尚未决策**的内容暂留各工程，差异明细见下文。

## 一、已移动到 common/ 的内容 ✅

| common/ 内路径 | 说明 | 统一版本依据 |
|---|---|---|
| `common/arm-none-eabi.ini` | meson 交叉编译配置 | 两边逐字节一致 |
| `common/meson_options.txt` | chip/time 构建选项 | 两边逐字节一致 |
| `common/SEGGER_RTT/` | RTT 库（整目录） | 两边逐字节一致 |
| `common/Driver/HAL/` | HAL 抽象层（整目录） | 两边逐字节一致 |
| `common/Driver/Hardware/` | 硬件驱动（整目录） | 两边逐字节一致 |
| `common/Driver/serial_send.hpp` | RS485 串口发送 | **APP 版**（超集，多 `send_raw_485`/`set_485_baudrate`） |
| `HAL::gd32f4::DAC`（合并进 `common/Driver/HAL/Platform/gd32f4.hpp`） | DAC0 驱动 | **APP 版**（完整初始化：软件触发+输出缓冲），以 `struct DAC` 组件形式并入 HAL 命名空间 |
| `common/Function/flash_param.hpp` | 外部 Flash 参数区读写（归 Function 层） | **BootLoader 版**（`namespace FlashParam` 封装） |
| `common/Driver/device_init.hpp` | 设备全局初始化统一入口 | **APP 完整版**（含 `work_status_led`/`READ_BACK_DAC`/`__enable_irq`） |
| `common/Driver/hardware.hpp` | 引脚/外设定义（归 Driver 层） | **APP 版**（`system_status_led`/`work_status_led`/`gd30ad3340_on_i2c0`） |
| `common/Function/params.hpp` | 设备参数结构体与工具函数 | **APP 版**（自由函数，无命名空间/全局状态） |
| `common/Protocol/` | 协议帧解析与应答构建 | **APP 版**（header-only：`ProtocolParser`/`ProtocolStatus`/`ProtocolFrame`/`ResponseBuilder`） |
| `common/CherryRB/` | 环形缓冲区库 | **APP 版**（`chry_ringbuffer.hpp` 用 `static inline`） |
| `common/template_schedule/` | 任务调度模板 | APP 独有，移动共享 |
| `common/freemodbus/` | FreeModbus 1.6.0 上游源码 | Git 子模块，固定到标签 `1.6.0` |
| `common/FreeModbus/` | FreeModbus Meson 接入与 GD32F4 移植层 | APP 使用；USART0(PA9/PA10) + TIMER6 |

软链清单（相对路径，仓库可移植）：

- `APP/`：`arm-none-eabi.ini`、`meson_options.txt`、`SEGGER_RTT`、`CherryRB`、`template_schedule`、`Protocol`、`FreeModbus` → `../common/...`；`Driver/HAL`、`Driver/Hardware`、`Driver/{serial_send,hardware,device_init}.hpp`、`Driver/flash_param.hpp`→`../../common/Function/...`、`Function/{hardware,params,flash_param}.hpp` → `../../common/...`
- `BootLoader/`：同 APP 侧结构

> 注：`dac_driver.hpp` 并入 HAL 目录后，`Driver/` 位置不再有软链，通过 HAL 依赖的 include 目录（`-IDriver/HAL`）以 `#include "dac_driver.hpp"` 解析。
> 注：`meson.build` 中 `subdir('Driver'/'HAL')` 等引用无需修改，meson 可解析符号链接路径。

## 二、已决策统一 — 记录（✅ 2026-08-05）

### 决策 1（第二轮）：serial_send / dac_driver / hardware / chry_ringbuffer 用 APP 版；template_schedule 移动

**BootLoader 调用方适配**（统一为 APP 版后必须的联动修改）：

| 文件 | 原符号 | 改为 |
|---|---|---|
| `BootLoader/Driver/device_init.hpp` | `LED::init()` | `system_status_led::init()` |
| `BootLoader/Driver/device_init.hpp` | `ADC g_adc;` / `ADC::MUX_...` 等 | `gd30ad3340_on_i2c0 g_adc;` / `gd30ad3340_on_i2c0::MUX_...` |
| `BootLoader/Function/device_state.hpp` | `LED::set()/clear()` | `system_status_led::set()/clear()` |
| `BootLoader/Function/main.cpp` | `ADC g_adc;` | `gd30ad3340_on_i2c0 g_adc;` |

> 对应关系：BL 版 `LED`（PE3）= APP 版 `system_status_led`（PE3），物理引脚相同。

### 决策 2（第三轮）：flash_param 用 BootLoader 版；params 用 APP 版；硬件初始化公用

**`flash_param.hpp`（namespace FlashParam）— APP 调用方适配**：

| 文件 | 原调用 | 改为 |
|---|---|---|
| `APP/Function/device.hpp` | `flash_param_save/load(params_)` | `FlashParam::save/load(params_)` |
| `APP/Function/command_handler.hpp` | `flash_param_save(params_)` | `FlashParam::save(params_)` |
| `APP/Function/alarm_manager.hpp` | `flash_param_read/erase_sector/write(...)` | `FlashParam::read/erase_sector/write(...)` |

**`params.hpp`（APP 版自由函数）— BootLoader 调用方适配**：

| 文件 | 原逻辑 | 改为 |
|---|---|---|
| `BootLoader/Function/main.cpp` | `Params::load()` / `Params::g_params` | 本地 `g_params` 实例 + `params_load()`/`params_set_defaults()`（用 `FlashParam::` + `params_crc32_calc`） |
| `BootLoader/Function/main.cpp` | 本地重复的 `baudrate_code_to_hz()` 定义 | 删除（统一版 params.hpp 已提供同名函数） |

> 注意：统一版 `params.hpp` 中 `#include "../Driver/flash_param.hpp"` 在 common/ 下恰好解析到 `common/Driver/flash_param.hpp`，无需修改。

**`device_init.hpp` 公用（APP 完整版）**：

- 新建 `common/Driver/device_init.hpp`：`device_init_all()` 采用 APP main 的完整初始化（`system_status_led`+`work_status_led`、I2C、OLED、外部 ADC、`ADC0_GPIO`+`READ_BACK_DAC`+`ADC0`、RTC、SPI Flash、USART1+CS_485、DAC、`__enable_irq`）
- `APP/Function/main.cpp`：初始化块替换为 `device_init_all()` 调用
- `BootLoader/Function/main.cpp`：原已调用 `device_init_all()`，行为升级为完整版（新增 `work_status_led`/`READ_BACK_DAC`/`__enable_irq`）

> 行为变化提示：BL 的 `device_init_all()` 从"简化版"升级为 APP 完整版（FLASH 34212 → 34276 B）；APP 无功能变化（原初始化块原样抽取）。

**BootLoader 调用方适配**（统一为 APP 版后必须的联动修改）：

| 文件 | 原符号 | 改为 |
|---|---|---|
| `BootLoader/Driver/device_init.hpp` | `LED::init()` | `system_status_led::init()` |
| `BootLoader/Driver/device_init.hpp` | `ADC g_adc;` / `ADC::MUX_...` 等 | `gd30ad3340_on_i2c0 g_adc;` / `gd30ad3340_on_i2c0::MUX_...` |
| `BootLoader/Function/device_state.hpp` | `LED::set()/clear()` | `system_status_led::set()/clear()` |
| `BootLoader/Function/main.cpp` | `ADC g_adc;` | `gd30ad3340_on_i2c0 g_adc;` |

> 对应关系：BL 版 `LED`（PE3）= APP 版 `system_status_led`（PE3），物理引脚相同。

**行为变化提示**（决策 1）：BootLoader 的 `dac_init` 由"简化版"改为 APP 版完整配置（软件触发 + 输出缓冲使能），FLASH 占用 33876 → 34212 B（+336 B）。

**行为变化提示**（决策 2）：BL 的 `device_init_all()` 从简化版升级为 APP 完整版，FLASH 34212 → 34276 B；BL 的 `Params::load()` 改为本地 `params_load()`（等价逻辑）。编译验证通过。

### 决策 3（第四轮）：Protocol/ 用 APP 版

**背景**：BL 原协议层为 `namespace Protocol`（`Parser`/`Status`/`Frame`/`Response`，声明 hpp + 实现拆 cpp）；APP 版为 header-only（`ProtocolParser`/`ProtocolStatus`/`ProtocolFrame`/`ResponseBuilder`）。两者帧格式完全一致（A5B6 头、B6A5 尾、ASCII hex、CRC-16-Modbus、13+content 结构），仅命名不同。

**改动**：

| 文件 | 原内容 | 改为 |
|---|---|---|
| `common/Protocol/` | （新建） | APP 版 3 个 header-only 文件 |
| `BootLoader/meson.build` | `executable(..., 'Protocol'/'protocol.cpp', ...)` | 移除（APP 版 header-only，无需编译 .cpp） |
| `BootLoader/Function/main.cpp` | `#include "../Protocol/protocol.hpp"` | `#include "../Protocol/protocol_parser.hpp"` + `response_builder.hpp` |
| `BootLoader/Function/main.cpp` | `Protocol::Parser<uart1_buffer> g_proto` | `ProtocolParser<uart1_buffer> g_proto` |
| `BootLoader/Function/main.cpp` | `Protocol::Frame` | `ProtocolFrame` |
| `BootLoader/Function/main.cpp` | `Protocol::Status::frame_ready` | `ProtocolStatus::frame_ready` |
| `BootLoader/Function/main.cpp` | `Protocol::Response::build_ok/build_error` | `ResponseBuilder::build_ok/build_error` |
| `BootLoader/Function/device_state.hpp` | `#include "Protocol/protocol.hpp"` | `#include "Protocol/protocol_parser.hpp"` |

> BL 的固件升级流程（0502/0503 命令、receive_firmware、execute_upgrade）逻辑不变，仅 API 改名。FLASH 占用不变（34276 B），编译验证通过。

### 决策 4（第五轮）：common 内部归属调整

用户指令：`hardware.hpp` 移动到 Driver；`flash_param.hpp` 移动到 Function；`dac_driver.hpp` **合并**到 HAL。

| 文件 | 原位置 | 新位置 |
|---|---|---|
| `hardware.hpp` | `common/Function/` | `common/Driver/hardware.hpp` |
| `flash_param.hpp` | `common/Driver/` | `common/Function/flash_param.hpp` |
| `dac_driver.hpp` | `common/Driver/` | **内容合并进 `common/Driver/HAL/Platform/gd32f4.hpp`**（作为 `HAL::gd32f4::DAC` 组件，原文件删除） |

**dac_driver 真正合并（非移动）**：

- 在 `Platform/gd32f4.hpp` 的 extern "C" 块增加 `#include "gd32f4xx_dac.h"`（DAC0 宏与 DAC 函数来源）
- **按 HAL 组件风格模板化**（与 `ADC_config`/`ADC`、`PWM_config`/`PWM` 同构）：
  - `registers` 增加 `DAC0_ADDR = DAC0`，undef 列表增加 `DAC0`/`DAC_BASE`（与 ADC0 处理一致）
  - `RCU_periph` 映射增加 `DAC0_ADDR → RCU_DAC` 分支
  - `DAC_config<DACx, out, trigger, wave, align>` 配置模板（全参数带默认值）
  - `DAC<DAC_CONFIG>` 设备模板：`init()`（软件触发+输出缓冲）、`set(uint16_t)`（0~4095 限幅）、`trigger()`（软件触发）
- `hardware.hpp` 增加 `DAC0_CONFIG`/`DAC0`（与 `ADC0_CONFIG`/`ADC0` 同构）
- 删除 `common/Driver/HAL/dac_driver.hpp` 独立文件

**调用方适配**：

| 文件 | 原调用 | 改为 |
|---|---|---|
| `common/Driver/device_init.hpp` | `#include "dac_driver.hpp"` + `dac_init()` | 移除 include；`DAC0::init()` |
| `APP/Function/main.cpp` | `#include "dac_driver.hpp"` | 移除（经 HAL 依赖传递） |
| `APP/Function/command_handler.hpp` | `#include "dac_driver.hpp"` + `dac_set(val)` + `dac_software_trigger_enable(DAC0, DAC_OUT0)` | 移除 include；`DAC0::set(val)` + `DAC0::trigger()` |

**include 适配**：

| 文件 | 原 include | 改为 |
|---|---|---|
| `common/Driver/serial_send.hpp` | `"Function/hardware.hpp"` | `"hardware.hpp"`（同 Driver 目录） |
| `common/Driver/device_init.hpp` | `"../Function/hardware.hpp"` | `"hardware.hpp"` |
| `common/Function/flash_param.hpp` | `"../Function/hardware.hpp"`、`"Hardware/gd25q.hpp"` | `"../Driver/hardware.hpp"`、`"gd25q.hpp"`（经 Hardware 依赖 include 目录） |
| `common/Function/params.hpp` | `"../Driver/flash_param.hpp"` | `"flash_param.hpp"`（同 Function 目录） |
| `APP/Function/main.cpp` | `"Driver/dac_driver.hpp"` | `"dac_driver.hpp"`（经 HAL include 目录） |
| `APP/Function/command_handler.hpp` | `"../Driver/dac_driver.hpp"` | `"dac_driver.hpp"` |

**软链调整**：`Function/{hardware}.hpp` 目标改指 `common/Driver/`；`Driver/{flash_param}.hpp` 目标改指 `common/Function/`；新增 `Driver/{hardware}.hpp`、`Function/{flash_param}.hpp` 软链（保持两工程目录结构兼容 include）；删除 `Driver/{dac_driver}.hpp` 软链（改经 HAL include 路径解析）。

> 编译验证通过，FLASH 占用不变（APP 44496 B / BL 34276 B）。

### 决策 5（第六轮）：删除 `BootLoader/Function/device_state.hpp`

**背景**：`device_state.hpp` 是 BootLoader 独有文件，其中 `DeviceState` 命名空间封装了 OLED 状态、LED、自动上报、采样数据、UTC、睡眠状态、浮点转换等工具。经检索，`main.cpp` 实际只用到 3 个符号：`g_oled_status`、`OLEDStatus::BOOTLOADER`、`oled_update()`。

**改动**：

| 文件 | 修改 |
|---|---|
| `BootLoader/Function/main.cpp` | 移除 `#include "device_state.hpp"`；将 `OLEDStatus` 枚举、`TEAM_ID`、`g_oled_status` 变量、`oled_update()` 函数内联进 main.cpp（去掉 `DeviceState::` 命名空间前缀）；调用改为 `g_oled_status = OLEDStatus::BOOTLOADER; oled_update()` |
| `BootLoader/Function/device_state.hpp` | **删除**（未使用的 LED/自动上报/采样/UTC/浮点转换等逻辑一并移除，BootLoader 无需这些功能） |

> 编译验证通过，FLASH 占用不变（34276 B）。注意：`main.cpp` 原已有同名 `read_u16` 静态函数，与 device_state.hpp 中的 `read_u16` 不同（后者随文件删除，无冲突）。

## 三、共有但内容不一致 — 尚未决策（❌ 暂留各工程实体文件）

### 1. `Driver/GD32F4xx_SPL/CMSIS/GD/GD32F4xx/Source/system_gd32f4xx.c`
- APP 版：`VECT_TAB_OFFSET = 0x11000`（APP 从 0x08011000 运行）
- BootLoader 版：`VECT_TAB_OFFSET = 0x00`（向量表在 0x08000000）
- **建议保留双版本不统一**（双固件布局核心差异），GD32F4xx_SPL 目录因此整体暂不移动。

### 2. `Driver/GD32F4xx_SPL/CMSIS/GD/GD32F4xx/Source/GCC/Ld/gd32f470xE_flash.ld`
- APP 版：栈 2K / 堆 1K
- BootLoader 版：栈 20K / 堆 10K + `PROVIDE(__heap_start/__heap_end)`
- **建议保留双版本不统一**（内存布局与固件强相关）。

### 3. `init.gdb`（三个版本各不相同）
- 根目录：`target/reset 2/load/reset 2`（4 行）
- BootLoader：`target/reset 2/load`（3 行）
- APP：含跳转 0x08011000 的调试脚本
- **建议保留各自版本**（调试脚本与各固件部署方式相关）。

## 四、各自独有内容（不涉及合并）

- **APP 独有**：`Function/{alarm_manager,command_handler,device}.hpp`
- **BootLoader 独有**：无（原 `Function/device_state.hpp` 已删除，OLED 状态逻辑并入 `main.cpp`）
- 两工程 `Function/main.cpp`、`app_flash.ld` / `bootloader_flash.ld`、`build/`（构建产物）、`.cache/`

## 五、待决策问题清单（剩余）

1. `system_gd32f4xx.c` / `gd32f470xE_flash.ld` / `init.gdb` — **建议保留双版本不统一**（双固件布局核心差异）
2. `Driver/GD32F4xx_SPL` 目录整体是否移入 common？（需先解决上述双版本差异）

> 决策完成后，可在 common 中建立统一版本，并将差异文件替换为软链；`system_gd32f4xx.c` 这类必须保留差异的，建议维持两工程实体文件。
