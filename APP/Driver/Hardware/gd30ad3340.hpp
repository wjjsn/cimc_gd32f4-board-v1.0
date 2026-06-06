#pragma once

#include <cstdint>
#include "bits_operation.hpp"

/**
 * @brief  GD30AD3340 驱动 (I2C 16位ADC)
 * @note   兼容 GD32F4xx HAL 框架的 I2C_device_addr 模板接口
 * @tparam I2C_Device I2C设备层 (例如 HAL::gd32f4::I2C_device_addr<bus_t, addr>)
 *
 * 使用示例:
 * @code
 *   using I2C_BUS  = HAL::gd32f4::I2C_bus<SDA_PIN, SCL_PIN, I2C0, 400000>;
 *   using ADC_I2C  = HAL::gd32f4::I2C_device_addr<I2C_BUS, 0x90>;
 *   GD30AD3340<ADC_I2C> adc;
 *
 *   I2C_BUS::init();
 *   adc.init();
 *   int16_t raw = adc.read_raw();
 *   float   v   = adc.read_voltage();
 * @endcode
 */
template <typename I2C_Device> class GD30AD3340 {
	static constexpr std::uint16_t DEFAULT_TIMEOUT = 1000;

	// 寄存器地址 (8-bit pointer)
	enum Reg : std::uint8_t {
		REG_CONVERSION = 0x00, // 转换结果寄存器 (只读)
		REG_CONFIG = 0x01, // 配置寄存器
		REG_LO_THRESH = 0x02, // 低阈值寄存器
		REG_HI_THRESH = 0x03, // 高阈值寄存器
	};

	// 配置寄存器的位域枚举
	enum ConfigBit : std::uint8_t {
		OS_BIT = 15, // 单次转换触发位 (写1启动, 自动清0)
		MUX_SHIFT = 12, // 输入多路选择器
		PGA_SHIFT = 9, // 可编程增益放大器
		MODE_BIT = 8, // 工作模式 (0=连续, 1=单次/掉电)
		DR_SHIFT = 5, // 数据速率
		COMP_MODE = 4, // 比较器模式
		COMP_POL = 3, // 比较器极性
		COMP_LAT = 2, // 比较器锁存
		COMP_QUE_SHIFT = 0, // 比较器队列 (11=禁用)
	};

	std::uint16_t config_reg_ = 0x4483; // 默认配置值

    public:
	// ======================== 枚举定义 ========================

	/// 输入多路选择器
	enum MUX : std::uint8_t {
		MUX_AIN0_AIN1 = 0b000, // 差分: AIN0(+)  AIN1(-)
		MUX_AIN0_AIN3 = 0b001, // 差分: AIN0(+)  AIN3(-)
		MUX_AIN1_AIN3 = 0b010, // 差分: AIN1(+)  AIN3(-)
		MUX_AIN2_AIN3 = 0b011, // 差分: AIN2(+)  AIN3(-)
		MUX_AIN0_GND = 0b100, // 单端: AIN0 对 GND
		MUX_AIN1_GND = 0b101, // 单端: AIN1 对 GND
		MUX_AIN2_GND = 0b110, // 单端: AIN2 对 GND
		MUX_AIN3_GND = 0b111, // 单端: AIN3 对 GND
	};

	/// 可编程增益 (PGA), 对应满量程电压
	enum PGA : std::uint8_t {
		PGA_6144 = 0b000, // ±6.144V
		PGA_4096 = 0b001, // ±4.096V
		PGA_2048 = 0b010, // ±2.048V (默认)
		PGA_1024 = 0b011, // ±1.024V
		PGA_0512 = 0b100, // ±0.512V
		PGA_0256 = 0b101, // ±0.256V
		PGA_0064 = 0b110, // ±0.064V (即 ±64mV)
	};

	/// 工作模式
	enum Mode : std::uint8_t {
		MODE_CONTINUOUS = 0, // 连续转换模式
		MODE_SINGLE = 1, // 单次转换/掉电模式
	};

	/// 数据速率
	enum DataRate : std::uint8_t {
		DR_6_25 = 0b000, //  6.25 SPS
		DR_12_5 = 0b001, // 12.5  SPS
		DR_25 = 0b010, // 25    SPS
		DR_50 = 0b011, // 50    SPS
		DR_100 = 0b100, // 100   SPS (默认)
		DR_250 = 0b101, // 250   SPS
		DR_500 = 0b110, // 500   SPS
		DR_1000 = 0b111, // 1000  SPS
	};

	/// 比较器队列 (转换次数后触发)
	enum CompQue : std::uint8_t {
		COMP_QUE_1 = 0b00, // 1次转换后触发
		COMP_QUE_2 = 0b01, // 2次转换后触发
		COMP_QUE_4 = 0b10, // 4次转换后触发
		COMP_QUE_DIS = 0b11, // 禁用比较器 (默认)
	};

	// ======================== 配置方法 ========================

	/** @brief 设置输入多路选择器 */
	void set_mux(MUX mux)
	{
		// 先清除 MUX 位 [14:12]
		BIT::CLR(config_reg_, 12);
		BIT::CLR(config_reg_, 13);
		BIT::CLR(config_reg_, 14);
		// 设置新值
		config_reg_ |= (static_cast<std::uint16_t>(mux) << MUX_SHIFT);
	}

	/** @brief 设置 PGA 增益 */
	void set_pga(PGA pga)
	{
		// 先清除 PGA 位 [11:9]
		BIT::CLR(config_reg_, 9);
		BIT::CLR(config_reg_, 10);
		BIT::CLR(config_reg_, 11);
		// 设置新值
		config_reg_ |= (static_cast<std::uint16_t>(pga) << PGA_SHIFT);
	}

	/** @brief 设置工作模式 */
	void set_mode(Mode mode)
	{
		if (mode == MODE_CONTINUOUS)
			BIT::CLR(config_reg_, MODE_BIT);
		else
			BIT::SET(config_reg_, MODE_BIT);
	}

	/** @brief 设置数据速率 */
	void set_data_rate(DataRate dr)
	{
		// 先清除 DR 位 [7:5]
		BIT::CLR(config_reg_, 5);
		BIT::CLR(config_reg_, 6);
		BIT::CLR(config_reg_, 7);
		// 设置新值
		config_reg_ |= (static_cast<std::uint16_t>(dr) << DR_SHIFT);
	}

	/** @brief 设置比较器队列 (触发次数) */
	void set_comparator_queue(CompQue que)
	{
		// 先清除 COMP_QUE 位 [1:0]
		BIT::CLR(config_reg_, 0);
		BIT::CLR(config_reg_, 1);
		// 设置新值
		config_reg_ |= static_cast<std::uint16_t>(que);
	}

	/// @brief 比较器模式 (高电平有效/窗口比较器等)
	void set_comparator_mode(bool polarity, bool latch)
	{
		if (polarity)
			BIT::SET(config_reg_, COMP_POL);
		else
			BIT::CLR(config_reg_, COMP_POL);
		if (latch)
			BIT::SET(config_reg_, COMP_LAT);
		else
			BIT::CLR(config_reg_, COMP_LAT);
	}

	/**
     * @brief  一键配置所有参数并写入芯片
     * @param  mux   输入选择
     * @param  pga   增益
     * @param  mode  工作模式
     * @param  dr    数据速率
     * @param  que   比较器队列
     * @retval None
     */
	void init(MUX mux = MUX_AIN0_GND, PGA pga = PGA_2048,
		  Mode mode = MODE_CONTINUOUS, DataRate dr = DR_100,
		  CompQue que = COMP_QUE_DIS)
	{
		config_reg_ = 0; // 从零开始构建
		set_mux(mux);
		set_pga(pga);
		set_mode(mode);
		set_data_rate(dr);
		set_comparator_queue(que);

		write_config();
	}

	// ======================== 底层 I2C 操作 ========================

	/** @brief 将当前 config_reg_ 写入芯片配置寄存器 */
	void write_config()
	{
		std::uint8_t buf[2] = {
			static_cast<std::uint8_t>(config_reg_ >> 8), // 高字节
			static_cast<std::uint8_t>(config_reg_ & 0xFF) // 低字节
		};
		I2C_Device::mem_write(REG_CONFIG, buf, 2, DEFAULT_TIMEOUT);
	}

	/** @brief 读取配置寄存器 */
	std::uint16_t read_config()
	{
		std::uint8_t buf[2] = { 0, 0 };
		I2C_Device::mem_read(REG_CONFIG, buf, 2, DEFAULT_TIMEOUT);
		config_reg_ = (static_cast<std::uint16_t>(buf[0]) << 8) |
			      buf[1];
		return config_reg_;
	}

	// ======================== 数据读取 ========================

	/**
     * @brief  读取原始 16 位有符号转换结果
     * @note   连续模式下直接读取最新值；单次模式下每次读取前需触发
     * @retval 16位有符号原始值 (读取失败返回 0)
     */
	std::int16_t read_raw()
	{
		std::uint8_t buf[2] = { 0, 0 };
		I2C_Device::mem_read(REG_CONVERSION, buf, 2, DEFAULT_TIMEOUT);
		std::uint16_t raw = (static_cast<std::uint16_t>(buf[0]) << 8) |
				    buf[1];
		return static_cast<std::int16_t>(raw);
	}

	/**
     * @brief  触发单次转换并读取结果 (仅单次模式可用)
     * @note   设置 OS=1 启动转换, 等待转换完成后读取
     * @retval 16位有符号原始值
     */
	std::int16_t read_single()
	{
		// 设置 OS 位为 1, 启动单次转换
		BIT::SET(config_reg_, OS_BIT);
		std::uint8_t buf[2] = {
			static_cast<std::uint8_t>(config_reg_ >> 8),
			static_cast<std::uint8_t>(config_reg_ & 0xFF)
		};
		I2C_Device::mem_write(REG_CONFIG, buf, 2, DEFAULT_TIMEOUT);

		// 等待 OS 位自动清 0 (表示转换完成)
		// 简单的轮询超时; 实际可用定时器中断优化
		constexpr uint32_t CONV_TIMEOUT = 100000;
		uint32_t timeout = CONV_TIMEOUT;
		do {
			I2C_Device::mem_read(REG_CONFIG, buf, 2,
					     DEFAULT_TIMEOUT);
			config_reg_ =
				(static_cast<std::uint16_t>(buf[0]) << 8) |
				buf[1];
			if (--timeout == 0)
				break;
		} while (BIT::READ(config_reg_, OS_BIT));

		// 读取转换结果
		return read_raw();
	}

	// ======================== 电压转换 ========================

	/**
     * @brief  读取并计算实际电压值
     * @param  vref  参考电压 (根据 PGA 设置传入对应满量程值)
     *               默认 2.048V (对应 PGA_2048)
     * @retval 电压值 (单位: V)
     * @note   单端输入: V = (raw / 32768.0) * vref
     *         差分输入: V = (raw / 32768.0) * vref
     */
	float read_voltage(float vref = 2.048f)
	{
		std::int16_t raw = read_raw();
		if (raw < 0)
			raw = 0; // 单端输入负值视为0
		return (static_cast<float>(raw) / 32768.0f) * vref;
	}

	/**
     * @brief  读取电压并通过经验公式转换为温度
     * @note   公式: T = -6.91*V^2 + 268.66*V - 279.28
     *         适用于 NTC 热敏电阻分压测量场景
     * @param  vref  参考电压 (同 read_voltage)
     * @retval 温度值 (单位: °C)
     */
	float read_temperature(float vref = 2.048f)
	{
		float V = read_voltage(vref);
		return -6.91f * V * V + 268.66f * V - 279.28f;
	}
};
