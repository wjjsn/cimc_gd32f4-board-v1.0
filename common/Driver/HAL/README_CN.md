[English](README.md)
# 硬件抽象层(HAL)库

## 概述
这是一个C++硬件抽象层(HAL)库，为不同的微控制器平台提供统一接口。目前支持：
- TI MSPM0系列
- STM32系列(已在STM32F1上测试)

## 功能特性
- GPIO操作(置位、清零、翻转、读取)
- 定时器控制(预分频、自动重装载、计数器)
- PWM生成(频率、占空比控制)
- 编译时类型检查和优化
- 跨平台的统一接口

## 设计原则
- 基于模板的实现，实现编译时优化
- 使用静态断言实现类型安全接口
- 最小运行时开销
- 跨平台的一致性API

## 使用示例

### GPIO控制
```cpp
// 用于MSPM0
using LED = HAL::mspm0::GPIO<GPIOA_BASE, DL_GPIO_PIN_5>;
LED::set();  // 打开LED
LED::toggle(); // 翻转LED状态

// 用于STM32
using BUTTON = HAL::stm32::GPIO<GPIOA_BASE, GPIO_PIN_0>;
bool state = BUTTON::read(); // 读取按钮状态
```

### PWM生成
```cpp
// 用于MSPM0
using MyPWM = HAL::mspm0::PWM<MyTimer, DL_TIMER_CC_INDEX_0, 1000>;
MyPWM::init(); // 以1kHz频率初始化
MyPWM::set_compare(500); // 设置50%占空比

// 用于STM32
using ServoPWM = HAL::stm32::PWM<ServoTimer, TIM_CHANNEL_1, 50>;
ServoPWM::init(); // 以50Hz频率初始化
```

## 依赖项
- MSPM0平台: TI MSPM0 DriverLib
- STM32平台: STM32 HAL库

## 构建说明
1. 将HAL目录包含到您的项目中
2. 包含相应的平台头文件(hal.hpp)
3. 链接平台特定的驱动库

## 支持平台
- TI MSPM0系列
- STM32系列(F1已测试，其他型号可能需要少量调整)
