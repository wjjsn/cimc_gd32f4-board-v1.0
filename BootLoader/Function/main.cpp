// ============================================================
// Bootloader — GD32F4 独立引导程序
// 地址空间: 0x08000000 - 0x0800FFFF (64K)
// ============================================================

#include "gd32f4xx.h"
#include "hardware.hpp"
#include "chry_ringbuffer.hpp"
#include "SEGGER_RTT.h"

// 协议层
#include "../Protocol/protocol_parser.hpp"
#include "../Protocol/response_builder.hpp"

// Driver 层
#include "../Driver/serial_send.hpp"
#include "../Driver/device_init.hpp"

// Function 层
#include "params.hpp"

// ===================== OLED 状态 =====================
enum class OLEDStatus : uint8_t {
	BOOTLOADER = 0,
	IDLE = 1,
	AUTO_SAMPLE = 2,
};

constexpr char TEAM_ID[] = "2026523446";

inline OLEDStatus g_oled_status = OLEDStatus::IDLE;

/// 刷新 OLED 双行显示
inline void oled_update()
{
	static OLEDStatus last_status =
		static_cast<OLEDStatus>(255); // 强制首次刷新

	if (g_oled_status == last_status)
		return;
	last_status = g_oled_status;

	g_screen.chear();
	g_screen.printf(0, 0, "%s", TEAM_ID);
	const char *line2 = "IDLE";
	switch (g_oled_status) {
	case OLEDStatus::BOOTLOADER:
		line2 = "Bootloader";
		break;
	case OLEDStatus::IDLE:
		line2 = "IDLE";
		break;
	case OLEDStatus::AUTO_SAMPLE:
		line2 = "AutoSample";
		break;
	default:
		break;
	}
	g_screen.printf(2, 0, "%s", line2);
	g_screen.update_force();
}

// ===================== 常量定义 =====================
constexpr uint32_t APP_START_ADDR = 0x08011000;
constexpr uint32_t STAGING_ADDR = 0x08051000;
constexpr uint32_t APP_SIZE = 128 * 1024;
constexpr uint32_t STAGING_SIZE = 7 * 1024;
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
ProtocolParser<uart1_buffer> g_proto;

// ===================== 全局外设实例 =====================
gd30ad3340_on_i2c0 g_adc;
Screen g_screen;

// ===================== 设备参数 =====================
DeviceParams g_params;

/// 恢复默认参数并写入 Flash
static void params_set_defaults()
{
	g_params.magic = PARAM_MAGIC;
	g_params.device_id = 0x0001;
	g_params.baudrate_code = 0x13; // 19200
	g_params.reserved0 = 0;
	g_params.ch0_ratio = 1.0f;
	g_params.ch1_ratio = 1.0f;
	g_params.ch0_threshold = 100.0f;
	g_params.ch1_threshold = 100.0f;
	g_params.ch2_threshold = 100.0f;
	g_params.alarm_mode = 0x02; // 不主动上报
	g_params.report_interval = 0x01; // 1s
	g_params.reserved1[0] = 0;
	g_params.reserved1[1] = 0;
	g_params.crc32 = 0;
	FlashParam::save(g_params);
}

/// 从 Flash 加载参数, 无效则恢复默认
static void params_load()
{
	FlashParam::load(g_params);
	if (g_params.magic != PARAM_MAGIC) {
		params_set_defaults();
		return;
	}
	uint32_t calc = params_crc32_calc(
		reinterpret_cast<const uint8_t *>(&g_params),
		sizeof(DeviceParams) - sizeof(uint32_t));
	if (calc != g_params.crc32)
		params_set_defaults();
}

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

static bool poll_frame(ProtocolFrame &frame)
{
	return g_proto.poll(frame) == ProtocolStatus::frame_ready;
}

static void send_ok(uint16_t devid, uint16_t cmd)
{
	uint8_t buf[64];
	uint16_t sz = ResponseBuilder::build_ok(devid, cmd, buf, sizeof(buf));
	if (sz)
		send_with_485(buf, sz);
}

static void send_error(uint16_t devid)
{
	uint8_t buf[64];
	uint16_t sz = ResponseBuilder::build_error(devid, buf, sizeof(buf));
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
	SEGGER_RTT_printf(0, "闪存: 擦除范围 0x%08lX ~ 0x%08lX (%lu 字节)\r\n",
			  start, start + size, size);
	fmc_unlock();
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
		       FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
	uint32_t erased = 0;
	for (uint32_t a = start; a < start + size; a += 4096) {
		uint32_t sec = addr_to_sector(a);
		SEGGER_RTT_printf(0, "闪存:   擦除扇区 %lu @ 0x%08lX\r\n", sec,
				  a);
		fmc_sector_erase(sec);
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_OPERR | FMC_FLAG_WPERR |
			       FMC_FLAG_PGMERR | FMC_FLAG_PGSERR);
		erased += 4096;
	}
	fmc_lock();
	SEGGER_RTT_printf(0, "闪存: 擦除完成, 共 %lu 字节\r\n", erased);
}

static void flash_write_word(uint32_t addr, uint32_t word)
{
	// RTT per word is too noisy; log only on first word of each 256B slice via flash_write_buffer
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
	SEGGER_RTT_WriteString(0, "升级: 等待 0502 命令 (开始升级)...\r\n");
	for (int sec = UPGRADE_TIMEOUT_S; sec >= 1; --sec) {
		char msg[64];
		snprintf(msg, sizeof(msg),
			 "wait for start Application(%ds)......\r\n", sec);
		send_with_485(msg);
		SEGGER_RTT_printf(0, "升级: 超时倒计时 %d 秒\r\n", sec);
		uint32_t dl = systick_tick_ms + 1000;
		while (systick_tick_ms < dl) {
			ProtocolFrame f;
			if (poll_frame(f) && f.size >= 8 && f.data[4] == 0x01 &&
			    read_u16(&f.data[5]) == 0x0502) {
				SEGGER_RTT_printf(
					0,
					"升级: 收到 0502! 设备ID=0x%04X, 进入升级流程\r\n",
					read_u16(&f.data[2]));
				return true;
			}
		}
	}
	SEGGER_RTT_WriteString(0, "升级: 0502 超时! 跳转到 APP.\r\n");
	return false;
}

/// 将字节缓冲区按 word 写入 Flash, 自动补齐不足 4 字节的尾部 (pad 0xFF)
static void flash_write_buffer(uint32_t addr, const uint8_t *data, uint32_t len)
{
	uint32_t aligned = (len + 3) & ~3u;
	SEGGER_RTT_printf(0,
			  "闪存: 写缓冲区到 0x%08lX, %lu 字节 (对齐后 %lu)\r\n",
			  addr, len, aligned);
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
static bool receive_firmware(uint16_t devid, uint32_t &total_written)
{
	SEGGER_RTT_printf(
		0, "升级: 开始接收固件, 设备ID=0x%04X, 暂存区 0x%08lX...\r\n",
		devid, STAGING_ADDR);

	uint8_t sbuf[SLICE_SIZE];
	uint32_t spos = 0, total = 0, lrt = systick_tick_ms;
	uint32_t last_log_kb = 0;

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
					uint32_t kb_now = total / 1024;
					if (kb_now != last_log_kb) {
						SEGGER_RTT_printf(
							0,
							"升级:   已接收 %lu KB...\r\n",
							kb_now);
						last_log_kb = kb_now;
					}
				}
				lrt = systick_tick_ms;
			} else {
				SEGGER_RTT_WriteString(
					0, "升级: 暂存区溢出! 固件过大.\r\n");
				return false;
			}
		}
		if ((systick_tick_ms - lrt > 500 && total > 0) ||
		    total >= STAGING_SIZE)
			break;
	}

	SEGGER_RTT_printf(0, "升级: 数据流结束. 总=%lu 字节, 尾部=%lu 字节\r\n",
			  total, spos);

	// 写入尾部不足 256 字节的剩余数据
	if (spos > 0) {
		SEGGER_RTT_printf(0, "升级: 写入尾部 %lu 字节到暂存区...\r\n",
				  spos);
		flash_write_buffer(STAGING_ADDR + total, sbuf, spos);
		total += spos;
	}

	total_written = total;
	uint32_t magic = *(volatile uint32_t *)STAGING_ADDR;
	SEGGER_RTT_printf(
		0,
		"升级: 固件接收完成: %u 字节, 魔术字=0x%08lX, 期望=0x%08lX\r\n",
		total, magic, MAGIC_WORD);
	bool ok = (magic == MAGIC_WORD);
	if (!ok) {
		SEGGER_RTT_WriteString(0,
				       "升级: 魔术字不匹配! 固件被拒绝.\r\n");
	}
	return ok;
}

/// 等待 0503 命令并执行固件搬运: 擦除 App 区 → 暂存区复制到 App 区
static bool execute_upgrade(uint16_t devid, uint32_t fw_size)
{
	SEGGER_RTT_WriteString(
		0, "升级: 魔术字校验通过, 发送 0502-OK 给上位机...\r\n");
	// 魔术字校验通过, 应答 OK
	send_ok(devid, 0x0502);
	SEGGER_RTT_WriteString(0, "升级: 等待 0503 命令 (提交升级)...\r\n");

	while (1) {
		ProtocolFrame f;
		if (poll_frame(f) && f.size >= 8 && f.data[4] == 0x01 &&
		    read_u16(&f.data[5]) == 0x0503) {
			SEGGER_RTT_printf(
				0,
				"升级: 收到 0503! 设备ID=0x%04X, 开始搬固件...\r\n",
				read_u16(&f.data[2]));
			send_ok(read_u16(&f.data[2]), 0x0503); //应答 OK

			SEGGER_RTT_WriteString(0, "升级: 擦除 APP 区域...\r\n");
			flash_erase_range(APP_START_ADDR, APP_SIZE);

			// 跳过魔术字(前4字节), 只搬运固件本体
			uint32_t fw_body = (fw_size >= 4) ? (fw_size - 4) : 0;
			uint32_t copy_end = (fw_body + 3) & ~3u;
			SEGGER_RTT_printf(
				0,
				"升级: 搬运 %lu 字节 从暂存区(0x%08lX+4) 到 APP(0x%08lX)...\r\n",
				copy_end, STAGING_ADDR, APP_START_ADDR);
			for (uint32_t off = 0; off < copy_end; off += 4) {
				flash_write_word(
					APP_START_ADDR + off,
					*(volatile uint32_t *)(STAGING_ADDR +
							       4 + off));
				if ((off & 0x3FF) == 0) { // log every 1KB
					SEGGER_RTT_printf(
						0,
						"升级:   已搬运 %lu / %lu 字节\r\n",
						off, copy_end);
				}
			}
			SEGGER_RTT_printf(
				0, "升级: 搬运完成! %lu 字节已写入 APP.\r\n",
				copy_end);
			return true;
		}
	}
}

static void run_upgrade_mode()
{
	SEGGER_RTT_WriteString(0, "=== 升级: 进入升级模式 ===\r\n");

	send_with_485("using command to interrupt start Application\r\n");
	int loop_count = 0;
	while (true) {
		flash_erase_range(STAGING_ADDR, STAGING_SIZE);
		SEGGER_RTT_printf(0, "升级: --- 升级循环 第 %d 轮 ---\r\n",
				  ++loop_count);
		if (!wait_for_upgrade_command()) {
			SEGGER_RTT_WriteString(
				0, "升级: 等待升级命令超时, 跳转到 APP.\r\n");
			jump_to_app();
			return;
		}

		uint16_t devid = g_params.device_id;
		uint32_t fw_size = 0;
		SEGGER_RTT_printf(
			0,
			"升级: 设备ID=0x%04X, 重置 UART 缓冲区准备接收固件...\r\n",
			devid);

		uart1_buffer::reset();
		if (receive_firmware(devid, fw_size)) {
			SEGGER_RTT_printf(
				0,
				"升级: 固件接收成功, fw_size=%lu, 进入固件搬运...\r\n",
				fw_size);
			auto execute_upgrade_stutus =
				execute_upgrade(devid, fw_size);
			SEGGER_RTT_WriteString(
				0, "升级: 固件搬运完成! 系统即将复位...\r\n");
			if (execute_upgrade_stutus) {
				break;
			}
		} else {
			SEGGER_RTT_WriteString(
				0,
				"升级: 固件接收失败! 发送错误应答, 重置缓冲区.\r\n");
			send_error(devid);
			// 魔术字校验失败, 丢弃后续数据
			uart1_buffer::reset();
		}
	}

	SEGGER_RTT_WriteString(0, "升级: 完成, 通过 NVIC 复位...\r\n");
	delay_ms(100);
	NVIC_SystemReset();
}

// ===================== main =====================
constexpr uint32_t DEFAULT_BAUDRATE = 19200;
extern "C" {
int main(void)
{
	SCB->VTOR = 0x08000000U;
	__DSB();
	__ISB();

	device_init_all();
	uart1_buffer::init();
	USART1::enable_it(0, 0);
	NVIC_SetPriority(USART1_IRQn, 0x00U);
	params_load();
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
			baudrate_code_to_hz(g_params.baudrate_code));
		usart_enable(HAL::gd32f4::registers::USART1_ADDR);
	// }
	rcu_all_reset_flag_clear();

	g_oled_status = OLEDStatus::BOOTLOADER;
	oled_update();

	if (SysTick_Config(SystemCoreClock / 1000U)) {
		while (1) {
			SEGGER_RTT_WriteString(0, "SysTick config failed!\r\n");
		}
	}
	NVIC_SetPriority(SysTick_IRQn, 0x01U);
	SEGGER_RTT_WriteString(0, "=== CIMC Bootloader ===\r\n");

	rcu_periph_clock_enable(RCU_BKPSRAM);
	pmu_backup_write_enable();
	bool up = (*BOOTLOADER_FLAG_ADDR == BOOTLOADER_MAGIC);
	*BOOTLOADER_FLAG_ADDR = 0;

	if (up) {
		run_upgrade_mode();
	} else
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
