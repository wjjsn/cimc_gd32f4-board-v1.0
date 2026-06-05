#pragma once

#include "../font.hpp"
#include <cstdio>
#include <cstring>
#include <cstdarg>
#define DEFAULT_TIMEOUT 1000

template <typename i2c_device_7bits>
class OLED
{
	uint8_t dataBuf1_[4][129] = {};
	uint8_t dataBuf2_[4][129] = {};
	static void write_command(std::uint8_t command)
	{
		std::uint8_t data[2] = {0x00, command};
		i2c_device_7bits::transmit(data, 2, DEFAULT_TIMEOUT);
	}
	static void write_data(std::uint8_t data)
	{
		std::uint8_t buffer[2] = {0x40, data};
		i2c_device_7bits::transmit(buffer, 2, DEFAULT_TIMEOUT);
	}
	void set_cursor(std::uint8_t Page, std::uint8_t X)
	{
		/*通过指令设置页地址和列地址*/
		write_command(0xB0 | Page);				 // 设置页位置
		write_command(0x10 | ((X & 0xF0) >> 4)); // 设置X位置高4位
		write_command(0x00 | (X & 0x0F));		 // 设置X位置低4位
	}
	void show_image(std::uint8_t Page, std::uint8_t X, std::uint8_t Width, std::uint8_t Height, const std::uint8_t *Image)
	{
		for (int j = 0; j < Height; ++j)
		{
			for (int i = 1; i < Width + 1; ++i)
			{
				dataBuf1_[Page + j][X + i] = Image[Width * j + (i - 1)];
			}
		}
	}
	void show_char(std::uint8_t Page, std::uint8_t X, char a)
	{
		set_cursor(Page, X);
		show_image(Page, X, 8, 2, EN8_16[a - ' ']);
	}
	void show_string(std::uint8_t Page, std::uint8_t X, char *String)
	{
		// for (int i = 0; String[i] != '\0'; ++i)
		// {
		// 	show_char(Page, X + i * 8, String[i]);
		// }
		uint16_t i = 0;
		char SingleChar[5];
		uint8_t CharLength = 0;
		uint16_t XOffset   = 0;
		uint16_t pIndex;

		while (String[i] != '\0') // 遍历字符串
		{
			/*此段代码的目的是，提取UTF8字符串中的一个字符，转存到SingleChar子字符串中*/
			/*判断UTF8编码第一个字节的标志位*/
			if ((String[i] & 0x80) == 0x00) // 第一个字节为0xxxxxxx
			{
				CharLength	  = 1;			 // 字符为1字节
				SingleChar[0] = String[i++]; // 将第一个字节写入SingleChar第0个位置，随后i指向下一个字节
				SingleChar[1] = '\0';		 // 为SingleChar添加字符串结束标志位
			}
			else if ((String[i] & 0xE0) == 0xC0) // 第一个字节为110xxxxx
			{
				CharLength	  = 2;				  // 字符为2字节
				SingleChar[0] = String[i++];	  // 将第一个字节写入SingleChar第0个位置，随后i指向下一个字节
				if (String[i] == '\0') { break; } // 意外情况，跳出循环，结束显示
				SingleChar[1] = String[i++];	  // 将第二个字节写入SingleChar第1个位置，随后i指向下一个字节
				SingleChar[2] = '\0';			  // 为SingleChar添加字符串结束标志位
			}
			else if ((String[i] & 0xF0) == 0xE0) // 第一个字节为1110xxxx
			{
				CharLength	  = 3; // 字符为3字节
				SingleChar[0] = String[i++];
				if (String[i] == '\0') { break; }
				SingleChar[1] = String[i++];
				if (String[i] == '\0') { break; }
				SingleChar[2] = String[i++];
				SingleChar[3] = '\0';
			}
			else if ((String[i] & 0xF8) == 0xF0) // 第一个字节为11110xxx
			{
				CharLength	  = 4; // 字符为4字节
				SingleChar[0] = String[i++];
				if (String[i] == '\0') { break; }
				SingleChar[1] = String[i++];
				if (String[i] == '\0') { break; }
				SingleChar[2] = String[i++];
				if (String[i] == '\0') { break; }
				SingleChar[3] = String[i++];
				SingleChar[4] = '\0';
			}
			else
			{
				i++; // 意外情况，i指向下一个字节，忽略此字节，继续判断下一个字节
				continue;
			}
			/*显示上述代码提取到的SingleChar*/
			if (CharLength == 1) // 如果是单字节字符
			{
				/*使用OLED_ShowChar显示此字符*/
				show_char(Page, X + XOffset, SingleChar[0]);
				XOffset += 8;
			}
			else // 否则，即多字节字符
			{
				/*遍历整个字模库，从字模库中寻找此字符的数据*/
				/*如果找到最后一个字符（定义为空字符串），则表示字符未在字模库定义，停止寻找*/
				for (pIndex = 0; std::strcmp(OLED_CF16x16[pIndex].Index, "") != 0; pIndex++)
				{
					/*找到匹配的字符*/
					if (std::strcmp(OLED_CF16x16[pIndex].Index, SingleChar) == 0)
					{
						break; // 跳出循环，此时pIndex的值为指定字符的索引
					}
				}
				/*将字模库OLED_CF16x16的指定数据以16*16的图像格式显示*/
				show_image(Page, X + XOffset, 16, 2, OLED_CF16x16[pIndex].Data);
				XOffset += 16;
			}
		}
	}

public:
	void init(void)
	{
		write_command(0xAE); /*display off*/
		write_command(0x00); /*set lower column address*/
		write_command(0x10); /*set higher column address*/
		write_command(0x00); /*set display start line*/
		write_command(0xB0); /*set page address*/
		write_command(0x81); /*contract control*/
		write_command(0xff); /*128*/
		write_command(0xA1); /*set segment remap*/
		write_command(0xA6); /*normal / reverse*/
		write_command(0xA8); /*multiplex ratio*/
		write_command(0x1F); /*duty = 1/32*/
		write_command(0xC8); /*Com scan direction*/
		write_command(0xD3); /*set display offset*/
		write_command(0x00);
		write_command(0xD5); /*set osc division*/
		write_command(0x80);
		write_command(0xD9); /*set pre-charge period*/
		write_command(0x1f);
		write_command(0xDA); /*set COM pins*/
		write_command(0x00);
		write_command(0xdb); /*set vcomh*/
		write_command(0x40);
		write_command(0x8d); /*set charge pump enable*/
		write_command(0x14);
		write_command(0xAF); // 开启显示
		for (int i = 0; i < 4; ++i)
		{
			dataBuf1_[i][0] = dataBuf2_[i][0] = 0x40;
		}
		chear();
		update_force();
	}
	void chear(void)
	{
		for (int i = 0; i < 4; ++i)
		{
			for (int j = 1; j < 129; ++j)
			{
				dataBuf1_[i][j] = 0x00;
			}
		}
	}
	void update(void)
	{
		for (int i = 0; i < 4; ++i)
		{
			if (std::memcmp(&dataBuf1_[i][0], &dataBuf2_[i][0], 129) != 0)
			{
				set_cursor(i, 0);
				i2c_device_7bits::transmit(&dataBuf1_[i][0], 129, DEFAULT_TIMEOUT);
				std::memcpy(&dataBuf2_[i][0], &dataBuf1_[i][0], 129);
			}
		}
	}
	void update_force(void)
	{
		for (int i = 0; i < 4; ++i)
		{
			set_cursor(i, 0);
			i2c_device_7bits::transmit(&dataBuf1_[i][0], 129, DEFAULT_TIMEOUT);
			std::memcpy(&dataBuf2_[i][0], &dataBuf1_[i][0], 129);
		}
	}
	void printf(std::uint8_t Page, std::uint8_t X, const char *format, ...)
	{
		std::va_list args;
		va_start(args, format);
		char buffer[25];
		std::vsnprintf(buffer, 25, format, args);
		va_end(args);
		show_string(Page, X, buffer);
	}
};
#undef DEFAULT_TIMEOUT