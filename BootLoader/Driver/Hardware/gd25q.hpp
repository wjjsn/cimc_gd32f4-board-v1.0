#pragma once

#include <cstdint>

/**
 * @brief  GD25Q SPI Flash 驱动 (静态类)
 * @note   模板参数 spi_device 是 HAL::gd32f4::SPI_device<BUS, CS>
 *         所有方法均为 static, 无需实例化
 */
template <typename spi_device>
struct GD25Q
{
	enum commands
	{
		WRITE_DATA			 = 0x02,
		WRITE_STATE_REGISTER = 0x01,
		WRITE_ENABLE		 = 0x06,

		READ_DATA			= 0x03,
		READ_STATE_REGISTER = 0x05,
		READ_ID				= 0x9F,

		ERASE_SECTOR = 0x20,
		ERASE_BILK	 = 0xC7,
		// 0x05?
		WIP_FLAG   = 0x01,
		DUMMY_BYTE = 0xA5, // byte for generate clock
		PAGE_SIZE  = 0x100
	};

private:
	static void write_enable()
	{
		std::uint8_t write_enable = WRITE_ENABLE;
		spi_device::transmit(&write_enable, 1);
	}
	static void wait_for_write_end()
	{
		// 命令 (0x05) 和状态读循环必须在同一 CS 窗口内,
		// 否则 flash 不会回状态字节, 读到的是 stale 数据
		std::uint8_t gd25q_status = 0;
		std::uint8_t read_status_register = READ_STATE_REGISTER;
		spi_device::select();
		spi_device::transmit_without_ctl_select(&read_status_register, 1);
		do {
			std::uint8_t dummy = DUMMY_BYTE;
			spi_device::transfer_without_ctl_select(&dummy, 1);
			gd25q_status = dummy;
		} while (1 == (gd25q_status & WIP_FLAG));
		spi_device::deselect();
	}

public:
	static std::uint32_t read_id()
	{
		// transfer() 一次拉低/拉高 CS，命令+地址+数据都在同一 CS 窗口内
		// SPI 全双工：同一缓冲区，发送值会被接收值覆盖
		std::uint8_t buf[4] = {READ_ID, DUMMY_BYTE, DUMMY_BYTE, DUMMY_BYTE};
		spi_device::transfer(buf, 4);
		return (buf[1] << 16) | (buf[2] << 8) | buf[3];
	}
	static void sector_erase(std::uint32_t sector_addr)
	{
		write_enable();
		std::uint8_t send_buf[4] = {ERASE_SECTOR,
									static_cast<std::uint8_t>(sector_addr >> 16),
									static_cast<std::uint8_t>(sector_addr >> 8),
									static_cast<std::uint8_t>(sector_addr >> 0)};
		spi_device::transmit(send_buf, 4);
		wait_for_write_end();
	}
	static void chip_erase()
	{
		write_enable();
		std::uint8_t chip_erase = ERASE_BILK;
		spi_device::transmit(&chip_erase, 1);
		wait_for_write_end();
	}
	static void page_write(std::uint8_t *pbuffer, std::uint32_t write_addr, std::uint16_t num_byte_to_write)
	{
		write_enable();
		std::uint8_t send_buf[4] = {WRITE_DATA,
									static_cast<std::uint8_t>(write_addr >> 16),
									static_cast<std::uint8_t>(write_addr >> 8),
									static_cast<std::uint8_t>(write_addr >> 0)};
		// 命令+地址 和 数据 需要在同一个 CS 窗口内，因此手动控 CS
		spi_device::select();
		spi_device::transmit_without_ctl_select(send_buf, 4);
		spi_device::transmit_without_ctl_select(pbuffer, num_byte_to_write);
		spi_device::deselect();
		wait_for_write_end();
	}
	static void block_write(std::uint8_t *pbuffer, std::uint32_t write_addr, std::uint16_t num_byte_to_write)
	{
		std::uint8_t num_of_page = 0, num_of_single = 0, addr = 0, count = 0, temp = 0;

		addr		  = write_addr % PAGE_SIZE;
		count		  = PAGE_SIZE - addr;
		num_of_page	  = num_byte_to_write / PAGE_SIZE;
		num_of_single = num_byte_to_write % PAGE_SIZE;

		/* write_addr is PAGE_SIZE aligned */
		if (0 == addr)
		{
			/* num_byte_to_write < PAGE_SIZE */
			if (0 == num_of_page)
			{
				page_write(pbuffer, write_addr, num_byte_to_write);
			}
			else
			{
				/* num_byte_to_write >= PAGE_SIZE */
				while (num_of_page--)
				{
					page_write(pbuffer, write_addr, PAGE_SIZE);
					write_addr += PAGE_SIZE;
					pbuffer += PAGE_SIZE;
				}
				page_write(pbuffer, write_addr, num_of_single);
			}
		}
		else
		{
			/* write_addr is not PAGE_SIZE aligned */
			if (0 == num_of_page)
			{
				/* (num_byte_to_write + write_addr) > PAGE_SIZE */
				if (num_of_single > count)
				{
					temp = num_of_single - count;
					page_write(pbuffer, write_addr, count);
					write_addr += count;
					pbuffer += count;
					page_write(pbuffer, write_addr, temp);
				}
				else
				{
					page_write(pbuffer, write_addr, num_byte_to_write);
				}
			}
			else
			{
				num_byte_to_write -= count;
				num_of_page	  = num_byte_to_write / PAGE_SIZE;
				num_of_single = num_byte_to_write % PAGE_SIZE;

				page_write(pbuffer, write_addr, count);
				write_addr += count;
				pbuffer += count;

				while (num_of_page--)
				{
					page_write(pbuffer, write_addr, PAGE_SIZE);
					write_addr += PAGE_SIZE;
					pbuffer += PAGE_SIZE;
				}
				if (0 != num_of_single)
				{
					page_write(pbuffer, write_addr, num_of_single);
				}
			}
		}
	}
	static void buffer_read(std::uint8_t *pbuffer, std::uint32_t read_addr, std::uint16_t num_byte_to_read)
	{
		// 命令+地址 和 数据 需要在同一个 CS 窗口内，因此手动控 CS
		std::uint8_t send_buf[4] = {READ_DATA,
									static_cast<std::uint8_t>(read_addr >> 16),
									static_cast<std::uint8_t>(read_addr >> 8),
									static_cast<std::uint8_t>(read_addr >> 0)};
		spi_device::select();
		spi_device::transmit_without_ctl_select(send_buf, 4);
		spi_device::receive_without_ctl_select(pbuffer, num_byte_to_read);
		spi_device::deselect();
	}
};
