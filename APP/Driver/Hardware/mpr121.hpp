#pragma once
/*修改于Adafruit*/
/*由于mpr121已停产，所以只提供简单支持*/
/*Because mpr121 has been discontinued, only simple support is provided*/
/*i2c address: 0x5A 0x5B 0x5C 0x5D*/
#include <cstdint>

// uncomment to use autoconfig !
// #define AUTOCONFIG // use autoconfig (Yes it works pretty well!)
template <typename i2c_device_7bits>
class mpr121
{
	static const std::uint16_t DEFAULT_TIMEOUT = 1000;
	enum reg
	{
		TOUCHSTATUS_L = 0x00,
		TOUCHSTATUS_H = 0x01,
		FILTDATA_0L	  = 0x04,
		FILTDATA_0H	  = 0x05,
		BASELINE_0	  = 0x1E,
		MHDR		  = 0x2B,
		NHDR		  = 0x2C,
		NCLR		  = 0x2D,
		FDLR		  = 0x2E,
		MHDF		  = 0x2F,
		NHDF		  = 0x30,
		NCLF		  = 0x31,
		FDLF		  = 0x32,
		NHDT		  = 0x33,
		NCLT		  = 0x34,
		FDLT		  = 0x35,

		TOUCHTH_0	 = 0x41,
		RELEASETH_0	 = 0x42,
		DEBOUNCE	 = 0x5B,
		CONFIG1		 = 0x5C,
		CONFIG2		 = 0x5D,
		CHARGECURR_0 = 0x5F,
		CHARGETIME_1 = 0x6C,
		ECR			 = 0x5E,
		AUTOCONFIG0	 = 0x7B,
		AUTOCONFIG1	 = 0x7C,
		UPLIMIT		 = 0x7D,
		LOWLIMIT	 = 0x7E,
		TARGETLIMIT	 = 0x7F,

		GPIODIR	   = 0x76,
		GPIOEN	   = 0x77,
		GPIOSET	   = 0x78,
		GPIOCLR	   = 0x79,
		GPIOTOGGLE = 0x7A,

		SOFTRESET = 0x80,
	};
	static void writeRegister(uint8_t reg, uint8_t value)
	{
		bool stop_required = true; // 判断是否需要进入停止状态指示位
		uint8_t ecr_backup;		   // 备份电极配置寄存器(ERC寄存器)
		i2c_device_7bits::mem_read(reg, i2c_device_7bits::MEMADD_SIZE_8BIT, &ecr_backup, 1, DEFAULT_TIMEOUT);
		uint8_t ecr_chear = 0x00;
		if ((reg == ECR) || ((0x73 <= reg) && (reg <= 0x7A))) { stop_required = false; } // 如果成立则无需进入停止状态
		if (stop_required)																 // 清除ERC寄存器，进入停止状态
		{
			i2c_device_7bits::mem_write(ECR, i2c_device_7bits::MEMADD_SIZE_8BIT, &ecr_chear, 1, DEFAULT_TIMEOUT);
		}
		i2c_device_7bits::mem_write(reg, i2c_device_7bits::MEMADD_SIZE_8BIT, &value, 1, DEFAULT_TIMEOUT); // 开始写入目标寄存器
		if (stop_required)																				  // 还原ERC寄存器的值
		{
			i2c_device_7bits::mem_write(ECR, i2c_device_7bits::MEMADD_SIZE_8BIT, &ecr_backup, 1, DEFAULT_TIMEOUT);
		}
	};
	static void setThresholds(uint8_t touch, uint8_t release)
	{
		for (uint8_t i = 0; i < 12; i++)
		{
			writeRegister(TOUCHTH_0 + 2 * i, touch);
			writeRegister(RELEASETH_0 + 2 * i, release);
		}
	}

public:
	static void init(uint8_t touch = 12, uint8_t release = 6)
	{
		writeRegister(SOFTRESET, 0x63); // 软重置
		writeRegister(ECR, 0x00);

		std::uint8_t c;
		i2c_device_7bits::mem_read(CONFIG2, i2c_device_7bits::MEMADD_SIZE_8BIT, &c, 1, DEFAULT_TIMEOUT);
		if (c != 0x24) { return; }

		setThresholds(touch, release);
		writeRegister(MHDR, 0x01);
		writeRegister(NHDR, 0x01);
		writeRegister(NCLR, 0x0E);
		writeRegister(FDLR, 0x00);

		writeRegister(MHDF, 0x01);
		writeRegister(NHDF, 0x05);
		writeRegister(NCLF, 0x01);
		writeRegister(FDLF, 0x00);

		writeRegister(NHDT, 0x00);
		writeRegister(NCLT, 0x00);
		writeRegister(FDLT, 0x00);

		writeRegister(DEBOUNCE, 0);
		writeRegister(CONFIG1, 0x10); // default, 16uA charge current
		writeRegister(CONFIG2, 0x20); // 0.5uS encoding, 1ms period

#ifdef AUTOCONFIG
		writeRegister(AUTOCONFIG0, 0x0B);

		// correct values for Vdd = 3.3V
		writeRegister(UPLIMIT, 200);	 // ((Vdd - 0.7)/Vdd) * 256
		writeRegister(TARGETLIMIT, 180); // UPLIMIT * 0.9
		writeRegister(LOWLIMIT, 130);	 // UPLIMIT * 0.65
#endif

		uint8_t ECR_SETTING = 0x8C;		 // 配置ECR寄存器，位的作用在数据手册的第16页
		writeRegister(ECR, ECR_SETTING); // start with above ECR setting
	}
	static uint16_t touched()
	{
		std::uint8_t buf[2];
		i2c_device_7bits::mem_read(TOUCHSTATUS_H, i2c_device_7bits::MEMADD_SIZE_8BIT, &buf[0], 1, DEFAULT_TIMEOUT);
		i2c_device_7bits::mem_read(TOUCHSTATUS_L, i2c_device_7bits::MEMADD_SIZE_8BIT, &buf[1], 1, DEFAULT_TIMEOUT);
		return (buf[0] << 8 | buf[1]);
	}
};