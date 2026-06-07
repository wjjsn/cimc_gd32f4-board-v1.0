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
constexpr uint32_t MAGIC_WORD =
	0x3CC3A55A; // ARM是小端序，《0x5AA5C33C》是错的;
constexpr uint32_t BOOT_TIMEOUT_S = 5;
constexpr uint32_t UPGRADE_TIMEOUT_S = 10;

#define BOOTLOADER_FLAG_ADDR ((volatile uint32_t *)0x40024000)
#define BOOTLOADER_MAGIC 0x424F4F54

// ===================== 环形缓冲区 =====================
chry_ringbuffer_t ctx_uart1_buffer;
using uart1_buffer = Cherry_RingBuffer<&ctx_uart1_buffer, 8192>;

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

/// 等待 0502 命令 (准备传输固件), 在 UPGRADE_TIMEOUT_S 秒内轮询
/// @return true = 收到 0502, false = 超时
static bool wait_for_upgrade_command()
{
	for (int sec = UPGRADE_TIMEOUT_S; sec >= 1; --sec) {
		char msg[64];
		snprintf(msg, sizeof(msg),
			 "wait for start Application(%ds)......\n", sec);
		send_with_485(msg);
		uint32_t dl = systick_tick_ms + 1000;
		while (systick_tick_ms < dl) {
			Protocol::Frame f;
			if (poll_frame(f) && f.size >= 8 && f.data[4] == 0x01 &&
			    read_u16(&f.data[5]) == 0x0502) {
				return true;
			}
		}
	}
	return false;
}

/// 将字节缓冲区按 word 写入 Flash, 自动补齐不足 4 字节的尾部 (pad 0xFF)
static void flash_write_buffer(uint32_t addr, const uint8_t *data, uint32_t len)
{
	uint32_t aligned = (len + 3) & ~3u;
	for (uint32_t i = 0; i < aligned; i += 4) {
		uint32_t w =
			data[i] |
			((uint32_t)(i + 1 < len ? data[i + 1] : 0xFF) << 8) |
			((uint32_t)(i + 2 < len ? data[i + 2] : 0xFF) << 16) |
			((uint32_t)(i + 3 < len ? data[i + 3] : 0xFF) << 24);
		flash_write_word(addr + i, w);
	}
}

/// 接收固件二进制数据到暂存区
/// 超时条件: 最后字节到达后 5s 无新数据, 或暂存区写满
/// @param[out] total_written  实际写入的字节数
/// @return true = 魔术字校验通过
static bool receive_firmware(uint32_t &total_written)
{
	flash_erase_range(STAGING_ADDR, STAGING_SIZE);

	uint8_t sbuf[SLICE_SIZE];
	uint32_t spos = 0, total = 0, lrt = systick_tick_ms;

	while (1) {
		uint8_t b;
		while (uart1_buffer::read_byte(&b)) {
			if (total < STAGING_SIZE) {
				sbuf[spos++] = b;
				if (spos >= SLICE_SIZE) {
					flash_write_buffer(STAGING_ADDR + total,
							   sbuf, SLICE_SIZE);
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

	// 写入尾部不足 256 字节的剩余数据
	if (spos > 0) {
		flash_write_buffer(STAGING_ADDR + total, sbuf, spos);
		total += spos;
	}

	total_written = total;
	return (*(volatile uint32_t *)STAGING_ADDR == MAGIC_WORD);
}

/// 等待 0503 命令并执行固件搬运: 擦除 App 区 → 暂存区复制到 App 区
static void execute_upgrade(uint16_t devid, uint32_t fw_size)
{
	send_ok(devid, 0x0502); // 魔术字校验通过, 应答 OK

	while (1) {
		Protocol::Frame f;
		if (poll_frame(f) && f.size >= 8 && f.data[4] == 0x01 &&
		    read_u16(&f.data[5]) == 0x0503) {
			send_ok(read_u16(&f.data[2]), 0x0503);
			flash_erase_range(APP_START_ADDR, APP_SIZE);
			// 跳过魔术字(前4字节), 只搬运固件本体
			uint32_t fw_body = (fw_size >= 4) ? (fw_size - 4) : 0;
			uint32_t copy_end = (fw_body + 3) & ~3u;
			for (uint32_t off = 0; off < copy_end; off += 4)
				flash_write_word(
					APP_START_ADDR + off,
					*(volatile uint32_t *)(STAGING_ADDR +
							       4 + off));
			break;
		}
	}
}

static void run_upgrade_mode()
{
	send_with_485("using command to interrupt start Application\r\n");

	if (!wait_for_upgrade_command()) {
		jump_to_app();
		return;
	}

	uint16_t devid = Params::g_params.device_id;
	uint32_t fw_size = 0;

	if (receive_firmware(fw_size)) {
		execute_upgrade(devid, fw_size);
	} else {
		send_error(devid);
	}

	delay_ms(100);
	NVIC_SystemReset();
}

// ===================== main =====================
constexpr uint32_t DEFAULT_BAUDRATE = 19200;
inline uint32_t baudrate_code_to_hz(uint8_t code)
{
	switch (code) {
	case 0x11:
		return 4800;
	case 0x12:
		return 9600;
	case 0x13:
		return 19200;
	case 0x14:
		return 115200;
	default:
		return 19200;
	}
}
extern "C" {
int main(void)
{
	device_init_all();
	uart1_buffer::init();
	USART1::enable_it(0, 0);
	Params::load();
	g_proto.init();
	// if ( //Power reset generated
	// 	RESET != rcu_flag_get(RCU_FLAG_PORRST) ||
	// 	//External PIN reset generated
	// 	(RESET != rcu_flag_get(RCU_FLAG_EPRST) &&
	// 	 RESET == rcu_flag_get(RCU_FLAG_PORRST))) {
	// 	usart_baudrate_set(HAL::gd32f4::registers::USART1_ADDR,
	// 			   DEFAULT_BAUDRATE);
	// 	usart_enable(HAL::gd32f4::registers::USART1_ADDR);
	// }
	// //Software reset generated
	// else if (RESET != rcu_flag_get(RCU_FLAG_SWRST)) {
		usart_baudrate_set(
			HAL::gd32f4::registers::USART1_ADDR,
			baudrate_code_to_hz(Params::g_params.baudrate_code));
		usart_enable(HAL::gd32f4::registers::USART1_ADDR);
	// }
	rcu_all_reset_flag_clear();

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

	g_screen.chear();

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
