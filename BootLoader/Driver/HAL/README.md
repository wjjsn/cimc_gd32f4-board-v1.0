[中文版](README_CN.md)
# Hardware Abstraction Layer (HAL) Library

## Overview
This is a C++ Hardware Abstraction Layer (HAL) library that provides unified interfaces for different microcontroller platforms. Currently supports:
- TI MSPM0 series
- STM32 series (tested with STM32F1)

## Features
- GPIO operations (set, clear, toggle, read)
- Timer control (prescaler, autoreload, counter)
- PWM generation (frequency, duty cycle control)
- Compile-time type checking and optimization
- Platform-independent interface

## Design Principles
- Template-based implementation for compile-time optimization
- Type-safe interfaces using static assertions
- Minimal runtime overhead
- Consistent API across different platforms

## Usage Examples

### GPIO Control
```cpp
// For MSPM0
using LED = HAL::mspm0::GPIO<GPIOA_BASE, DL_GPIO_PIN_5>;
LED::set();  // Turn on LED
LED::toggle(); // Toggle LED state

// For STM32
using BUTTON = HAL::stm32::GPIO<GPIOA_BASE, GPIO_PIN_0>;
bool state = BUTTON::read(); // Read button state
```

### PWM Generation
```cpp
// For MSPM0
using MyPWM = HAL::mspm0::PWM<MyTimer, DL_TIMER_CC_INDEX_0, 1000>;
MyPWM::init(); // Initialize with 1kHz frequency
MyPWM::set_compare(500); // Set 50% duty cycle

// For STM32
using ServoPWM = HAL::stm32::PWM<ServoTimer, TIM_CHANNEL_1, 50>;
ServoPWM::init(); // Initialize with 50Hz frequency
```

## Dependencies
- For MSPM0: TI MSPM0 DriverLib
- For STM32: STM32 HAL Library

## Building
1. Include the HAL directory in your project
2. Include the appropriate platform header (hal.hpp)
3. Link with the platform-specific driver library

## Supported Platforms
- TI MSPM0 series
- STM32 series (F1 tested, others may work with minor adjustments)
