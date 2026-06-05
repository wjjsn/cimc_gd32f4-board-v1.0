# HAL Platform 开发约束

## 概述

本项目为 **跨平台硬件抽象层(HAL)库**，核心约束位于 `subprojects/HAL/Platform/` 目录。所有平台实现必须遵循统一的设计模式与约束规范。

---

## 核心约束

### 1. 文件结构约束

```
Platform/
├── gd32f4.hpp
├── stm32.hpp
└── mspm0.hpp
```

新增平台：创建新文件 `Platform/<platform>.hpp`，并在 `hal.hpp` 中添加对应 `#include`。

---

### 2. 命名空间约束

每个平台必须使用独立的命名空间：

```cpp
namespace HAL
{
    namespace <platform>  // 例: gd32f4, stm32, mspm0
    {
        // 实现
    }
}
```

---

### 3. 组件约束 (必须实现的模板)

所有平台必须实现以下模板结构体，接口签名必须保持一致。

#### 3.1 GPIO 通用输入/输出

**必须方法**：

| 方法 | 返回类型 | 参数 | 说明 |
|------|----------|------|------|
| `init()` | `void` | — | 初始化 GPIO，含时钟使能 |
| `set()` | `void` | — | 置位引脚 |
| `clear()` | `void` | — | 清零引脚 |
| `toggle()` | `void` | — | 翻转引脚 |
| `read()` | `bool` | — | 读取引脚状态 |

---

#### 3.2 TIM 定时器

**必须方法**：

| 方法 | 返回类型 | 参数 | 说明 |
|------|----------|------|------|
| `init()` | `void` | — | 初始化定时器 |
| `start()` | `void` | — | 启动定时器 |
| `start_it()` | `void` | — | 启动定时器中断 |
| `set_prescaler()` | `void` | `uint32_t prescaler` | 设置预分频 |
| `set_autoreload()` | `void` | `uint32_t autoreload` | 设置自动重装载值 |
| `set_counter()` | `void` | `uint32_t counter` | 设置计数器值 |
| `get_autoreload()` | `uint32_t` | — | 获取自动重装载值 |
| `get_counter()` | `uint32_t` | — | 获取当前计数器值 |

---

#### 3.3 PWM 脉冲宽度调制

**必须方法**：

| 方法 | 返回类型 | 参数 | 说明 |
|------|----------|------|------|
| `init()` | `void` | — | 初始化 PWM，设置频率和占空比初值 |
| `start()` | `void` | — | 启动 PWM 输出 |
| `set_compare()` | `void` | `uint32_t compare` | 设置捕获比较值（占空比） |
| `set_frequency()` | `void` | `uint32_t frequency` | 设置 PWM 频率 |

**频率约束**：
```cpp
static_assert(!(frequency_ == 0 || frequency_ > 1000000), "PWM frequency must be in range 1-1000000 Hz");
```

#### 3.4 I2C

**外设层 `I2C_bus<SDA, SCL, I2Cx, clkspeed>`**

| 方法 | 参数 | 说明 |
|------|------|------|
| `init()` | — | 初始化总线 |
| `transmit()` | `uint8_t slave_addr, uint8_t* p, uint16_t n, uint32_t t` | 主发送（地址由调用方传入） |
| `receive()` | `uint8_t slave_addr, uint8_t* p, uint16_t n, uint32_t t` | 主接收 |

**设备层 `I2C_device_addr<bus_t, slave_address>`**（7-bit 地址）：

| 方法 | 参数 | 说明 |
|------|------|------|
| `init()` | — | 调 `bus_t::init()` |
| `transmit()` | `uint8_t* p, uint16_t n, uint32_t t` | 转发到 `bus.transmit(addr, ...)` |
| `receive()` | `uint8_t* p, uint16_t n, uint32_t t` | 转发到 `bus.receive(addr, ...)` |
| `mem_write()` | `uint16_t reg, uint8_t* p, uint16_t n, uint32_t t` | 写寄存器 |
| `mem_read()` | `uint16_t reg, uint8_t* p, uint16_t n, uint32_t t` | 读寄存器 |


#### UART

| 方法 | 返回类型 | 参数 | 说明 |
|------|----------|------|------|
| `init()` | `void` | — | 初始化 UART，波特率等配置 |
| `transmit()` | `void` | `const uint8_t* pData, uint16_t Size, uint32_t Timeout` | 发送数据 |
| `receive()` | `void` | `uint8_t* pData, uint16_t Size, uint32_t Timeout` | 接收数据 |

#### SPI
**SPI 主/从设备**：

**外设层 `SPI_config<SPIx, psc, ckp, mode, nss, transmode, framesize, endian>`**（仅 constexpr 字段，**不**提供行为）：

| 参数 | 默认 |
|------|------|
| `SPIx` | — |
| `prescaler` | `SPI_PSC_64` |
| `clock_polarity_phase` | `SPI_CK_PL_LOW_PH_2EDGE` |
| `device_mode` | `SPI_MASTER` |
| `nss` | `SPI_NSS_SOFT` |
| `transmode` | `SPI_TRANSMODE_FULLDUPLEX` |
| `framesize` | `SPI_FRAMESIZE_8BIT` |
| `endian` | `SPI_ENDIAN_MSB` |

**外设层 `SPI<MOSI, MISO, SCLK, SPI_CONFIG>`**（MOSI/MISO/SCLK 配 AF5，**不**含 CS）：

| 方法 | 参数 | 说明 |
|------|------|------|
| `init()` | — | 初始化 |
| `transmit()` | `uint8_t* p, uint16_t n, uint32_t t` | 主发送（**不**控 CS） |
| `receive()` | `uint8_t* p, uint16_t n, uint32_t t` | 主接收（**不**控 CS） |
| `transfer()` | `uint8_t* p, uint16_t n, uint32_t t` | 全双工（**不**控 CS） |

**设备层 `SPI_device<bus_t, GPIO_CS>`**

| 方法 | 参数 | 说明 |
|------|------|------|
| `init()` | — | 初始化总线 + CS，CS 默认高 |
| `select()` / `deselect()` | — | 裸 API：手动控 CS 时序 |
| `transmit()` | `uint8_t* p, uint16_t n, uint32_t t` | 拉低 CS → bus::transmit → 拉高 CS |
| `receive()` | `uint8_t* p, uint16_t n, uint32_t t` | 拉低 CS → bus::receive → 拉高 CS |
| `transfer()` | `uint8_t* p, uint16_t n, uint32_t t` | 拉低 CS → bus::transfer → 拉高 CS |
| `transmit_without_ctl_select()` | `uint8_t* p, uint16_t n, uint32_t t` | bus::transmit |
| `receive_without_ctl_select()` | `uint8_t* p, uint16_t n, uint32_t t` | bus::receive |
| `transfer()_without_ctl_select` | `uint8_t* p, uint16_t n, uint32_t t` | bus::transfer |

**ADC 模数转换**：

| 方法 | 返回类型 | 参数 | 说明 |
|------|----------|------|------|
| `init()` | `void` | — | 初始化 ADC，采样率等配置 |
| `start()` | `void` | — | 启动 ADC 转换 |
| `start_it()` | `void` | — | 启动 ADC 中断转换 |
| `get_value()` | `uint32_t` | — | 获取转换结果 |
| `get_channel()` | `uint32_t` | — | 获取当前通道 |

#### 3.5 可选组件

根据平台能力添加，以下为接口规范：


**CRC 校验**：

| 方法 | 返回类型 | 参数 | 说明 |
|------|----------|------|------|
| `init()` | `void` | — | 初始化 CRC 多项式等配置 |
| `calculate()` | `uint32_t` | `uint8_t* pData, uint16_t Size` | 计算 CRC 值 |
| `reset()` | `void` | — | 重置 CRC 状态 |

---

### 4. GPIO 模板变体示例

GD32F4xx 平台展示了三种 GPIO 特化模式，新增平台应参考：

```cpp
// 变体1: 输出模式 (OutputConfig 特化)
template <...>
struct GPIO<GPIOx, Pin, GPIO_MODE_OUTPUT, PULL, OutputConfig<...>> { ... };

// 变体2: 复用功能模式 (AFConfig 特化)
template <...>
struct GPIO<GPIOx, Pin, GPIO_MODE_AF, PULL, AFConfig<...>> { ... };

// 变体3: 输入模式
template <...>
struct GPIO<GPIOx, Pin, GPIO_MODE_INPUT, PULL, void> { ... };
```

**其他平台可简化**：STM32/MSPM0 使用简单 GPIO 模板，不做特化区分。

---

### 5. 编译时检查约束

所有错误尽量在编译期捕获。常见做法：

- **兜底主模板**：未特化的主模板里写 `static_assert(false, "...")`，阻止误用。
- **常量范围**：模板参数为数值时，在类体内用 `static_assert` 校验合法区间。
- **类型/概念约束**：形如"必须是 AF4 引脚"这类跨字段条件，用 C++20 `requires` 或 `concept`，错误指向调用点而不是模板深处。

```cpp
// 兜底主模板
template <...>
struct GPIO
{
    static_assert((false), "fallback");
};

// 常量范围
static_assert(!(frequency == 0 || frequency > 1000000),
              "PWM frequency must be in range 1-1000000 Hz");

// 跨字段约束
template <typename SDA, typename SCL, ...>
    requires(SDA::af_config::af_num == GPIO_AF_4 &&
             SCL::af_config::af_num == GPIO_AF_4)
struct I2C_device_7bits { ... };
```
