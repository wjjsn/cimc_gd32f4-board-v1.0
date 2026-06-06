// ============================================================
// Bootloader — GD32F4 独立引导程序
// 地址空间: 0x08000000 - 0x0800FFFF (64K)
// ============================================================

#include "hardware.hpp"
#include "chry_ringbuffer.hpp"
#include "SEGGER_RTT.h"

// 协议层
#include "../Protocol/protocol.hpp"

// Driver 层
#include "../Driver/serial_send.hpp"
#include "../Driver/device_init.hpp"

// Function 层
#include "params.hpp"
#include "device_state.hpp"

// ===================== 常量定义 =====================
constexpr uint32_t APP_START_ADDR = 0x08011000;
constexpr uint32_t STAGING_ADDR = 0x08051000;
constexpr uint32_t APP_SIZE = 128 * 1024;
constexpr uint32_t STAGING_SIZE = 128 * 1024;
constexpr uint32_t SLICE_SIZE = 256;
constexpr uint32_t MAGIC_WORD = 0x5AA5C33C;
constexpr uint32_t BOOT_TIMEOUT_S = 5;
constexpr uint32_t UPGRADE_TIMEOUT_S = 10;

#define BOOTLOADER_FLAG_ADDR ((volatile uint32_t *)0x40024000)
#define BOOTLOADER_MAGIC 0x424F4F54

// ===================== 环形缓冲区 =====================
chry_ringbuffer_t ctx_uart1_buffer;
using uart1_buffer = Cherry_RingBuffer<&ctx_uart1_buffer, 128>;
template <>
std::array<uint8_t, 128> Cherry_RingBuffer<&ctx_uart1_buffer, 128>::pool_{};

// ===================== 协议解析器 =====================
Protocol::Parser<uart1_buffer> g_proto;

// ===================== 全局外设实例 =====================
ADC g_adc;
Screen g_screen;

// ===================== SysTick =====================
static volatile uint32_t systick_tick_ms = 0;

uint64_t systick_get_ms()
{
	return systick_tick_ms;
}

// ===================== 辅助函数 =====================

static inline uint16_t read_u16(const uint8_t *p)
{
	return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}

static void delay_ms(uint32_t ms)
{
	uint32_t deadline = systick_tick_ms + ms;
	while (systick_tick_ms < deadline)
		__asm__ volatile("nop");
}

static bool poll_frame(Protocol::Frame &frame)
{
	return g_proto.poll(frame) == Protocol::Status::frame_ready;
}

static void send_ok(uint16_t devid, uint16_t cmd)
{
	uint8_t buf[64];
	uint16_t sz =
		Protocol::Response::build_ok(devid, cmd, buf, sizeof(buf));
	if (sz)
		send_with_485(buf, sz);
}

static void send_error(uint16_t devid)
{
	uint8_t buf[64];
	uint16_t sz = Protocol::Response::build_error(devid, buf, sizeof(buf));
	if (sz)
		send_with_485(buf, sz);
}

/// 内部 Flash 地址 → 扇区编号 (GD32F4xE 512KB)
static uint32_t addr_to_sector(uint32_t addr)
{
	if (addr < 0x08004000)
		return CTL_SECTOR_NUMBER_0; // 16KB
	if (addr < 0x08008000)
		return CTL_SECTOR_NUMBER_1; // 16KB
	if (addr < 0x0800C000)
		return CTL_SECTOR_NUMBER_2; // 16KB
	if (addr < 0x08010000)
		return CTL_SECTOR_NUMBER_3; // 16KB
	if (addr < 0x08020000)
		return CTL_SECTOR_NUMBER_4; // 64KB
	if (addr < 0x08040000)
		return CTL_SECTOR_NUMBER_5; // 128KB
	if (addr < 0x08060000)
		return CTL_SECTOR_NUMBER_6; // 128KB
	return CTL_SECTOR_NUMBER_7; // 128KB
}

static void flash_erase_range(uint32_t start, uint32_t size)
{
	fmc_unlock();
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
		       FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
	for (uint32_t a = start; a < start + size; a += 4096) {
		fmc_sector_erase(addr_to_sector(a));
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
			       FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
	}
	fmc_lock();
}

static void flash_write_word(uint32_t addr, uint32_t word)
{
	fmc_unlock();
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
		       FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
	fmc_word_program(addr, word);
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
		       FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
	fmc_lock();
}

__attribute__((naked, noreturn)) static void jump_to_app_entry(uint32_t app_sp,
							       uint32_t app_pc)
{
	__asm__ volatile("msr msp, r0\n"
			 "bx r1\n");
}

static void jump_to_app()
{
	uint32_t app_sp = *(volatile uint32_t *)APP_START_ADDR;
	uint32_t app_pc = *(volatile uint32_t *)(APP_START_ADDR + 4);
	if (app_sp < 0x20000000 || app_sp > 0x20030000 || (app_sp & 0x7) != 0 ||
	    app_pc < APP_START_ADDR || app_pc >= APP_START_ADDR + APP_SIZE ||
	    (app_pc & 0x1) == 0) {
		SEGGER_RTT_WriteString(0, "BOOT: Invalid APP, halt\r\n");
		while (1)
			__asm__ volatile("nop");
	}
	__disable_irq();
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;
	SCB->VTOR = APP_START_ADDR;
	__DSB();
	__ISB();
	jump_to_app_entry(app_sp, app_pc);
}

// ===================== 升级流程 =====================

static void run_upgrade_mode()
{
	send_with_485("using command to interrupt start Application\r\n");
	bool started = false;

	for (int sec = UPGRADE_TIMEOUT_S; sec >= 1; --sec) {
		char msg[64];
		snprintf(msg, sizeof(msg),
			 "wait for start Application(%ds)……\r\n", sec);
		send_with_485(msg);
		uint32_t dl = systick_tick_ms + 1000;
		while (systick_tick_ms < dl) {
			Protocol::Frame f;
			if (poll_frame(f) && f.size >= 8 && f.data[4] == 0x01 &&
			    read_u16(&f.data[5]) == 0x0502) {
				started = true;
				goto recv_fw;
			}
		}
	}

recv_fw:
	if (!started) {
		jump_to_app();
		return;
	}

	uint16_t devid = Params::g_params.device_id;
	send_ok(devid, 0x0502);
	flash_erase_range(STAGING_ADDR, STAGING_SIZE);

	uint8_t sbuf[SLICE_SIZE];
	uint32_t spos = 0, total = 0, lrt = systick_tick_ms;

	while (1) {
		uint8_t b;
		while (uart1_buffer::read_byte(&b)) {
			if (total < STAGING_SIZE) {
				sbuf[spos++] = b;
				if (spos >= SLICE_SIZE) {
					for (uint32_t i = 0; i < SLICE_SIZE;
					     i += 4)
						flash_write_word(
							STAGING_ADDR + total +
								i,
							sbuf[i] |
								((uint32_t)
									 sbuf[i +
									      1]
								 << 8) |
								((uint32_t)
									 sbuf[i +
									      2]
								 << 16) |
								((uint32_t)
									 sbuf[i +
									      3]
								 << 24));
					total += SLICE_SIZE;
					spos = 0;
				}
				lrt = systick_tick_ms;
			}
		}
		if ((systick_tick_ms - lrt > 5000 && total > 0) ||
		    total >= STAGING_SIZE)
			break;
	}

	if (*(volatile uint32_t *)STAGING_ADDR == MAGIC_WORD) {
		send_ok(devid, 0x0502);
		while (1) {
			Protocol::Frame f;
			if (poll_frame(f) && f.size >= 8 && f.data[4] == 0x01 &&
			    read_u16(&f.data[5]) == 0x0503) {
				send_ok(read_u16(&f.data[2]), 0x0503);
				flash_erase_range(APP_START_ADDR, APP_SIZE);
				for (uint32_t off = 0; off < total; off += 4)
					flash_write_word(
						APP_START_ADDR + off,
						*(volatile uint32_t
							  *)(STAGING_ADDR +
							     off));
				break;
			}
		}
	} else {
		send_error(devid);
		while (1) {
			Protocol::Frame f;
			poll_frame(f);
		}
	}

	delay_ms(100);
	NVIC_SystemReset();
}

// ===================== main =====================
extern "C" {
int main(void)
{
	device_init_all();
	uart1_buffer::init();
	USART1::enable_it(0, 0);
	Params::load();
	g_proto.init();

	DeviceState::g_oled_status = DeviceState::OLEDStatus::BOOTLOADER;
	DeviceState::oled_update();

	if (SysTick_Config(SystemCoreClock / 1000U)) {
		while (1) {
			SEGGER_RTT_WriteString(0, "SysTick config failed!\r\n");
		}
	}
	NVIC_SetPriority(SysTick_IRQn, 0x00U);
	SEGGER_RTT_WriteString(0, "=== CIMC Bootloader ===\r\n");

	rcu_periph_clock_enable(RCU_BKPSRAM);
	pmu_backup_write_enable();
	bool up = (*BOOTLOADER_FLAG_ADDR == BOOTLOADER_MAGIC);
	*BOOTLOADER_FLAG_ADDR = 0;

	if (up)
		run_upgrade_mode();
	else
		delay_ms(BOOT_TIMEOUT_S * 1000);

	jump_to_app();
	while (1)
		__asm__ volatile("nop");
}

void USART1_IRQHandler()
{
	if (usart_interrupt_flag_get(HAL::gd32f4::registers::USART1_ADDR,
				     USART_INT_FLAG_RBNE) == SET)
		uart1_buffer::write_byte(usart_data_receive(
			HAL::gd32f4::registers::USART1_ADDR));
}

void SysTick_Handler(void)
{
	systick_tick_ms = systick_tick_ms + 1U;
}
void _exit(int)
{
	while (1)
		__asm__ volatile("nop");
}
}
