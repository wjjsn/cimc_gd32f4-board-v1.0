#include "Driver/hardware.hpp"
#include "modbus_config.hpp"

#include <array>

extern "C" {
#include "mb.h"
#include "mbport.h"

void USART0_IRQHandler(void);
void TIMER6_IRQHandler(void);
}

namespace
{
/*
 * 这是已经过实机验证的GD32F4底层移植，现场只换寄存器表时通常不要修改。
 *
 * 接收路径：USART0 -> DMA1 Channel2 -> 字节/超时事件队列 -> FreeModbus FSM。
 * USART IDLE中断负责尽快收割一批DMA数据，主循环再按DMA剩余计数做兜底。保留两条
 * 路径是因为这块板上曾出现RBNE/IDLE状态已置位但NVIC请求不稳定的现象。
 */
volatile uint8_t received_byte;
bool transmitter_enabled;
bool replaying_rx_event;

// 官方V3.3.3 USART IDLE示例使用256字节、8位、非循环DMA，本项目保持相同配置。
constexpr uint16_t rx_dma_capacity = 256U;
std::array<uint8_t, rx_dma_capacity> rx_dma_buffer;
uint16_t rx_dma_consumed;
bool rx_dma_enabled;

enum class RxEventType : uint8_t {
	byte,
	timeout,
};

struct RxEvent {
	RxEventType type;
	uint8_t byte;
};

constexpr uint16_t rx_event_capacity = 512U;
constexpr uint16_t rx_event_mask = rx_event_capacity - 1U;
static_assert((rx_event_capacity & rx_event_mask) == 0U);

std::array<RxEvent, rx_event_capacity> rx_events;
volatile uint16_t rx_event_head;
volatile uint16_t rx_event_tail;
volatile bool rx_event_overflow;

void reset_rx_events()
{
	// USART0和TIMER6都是生产者，清队列时必须短暂屏蔽中断，避免指针清到一半被打断。
	const uint32_t primask = __get_PRIMASK();
	__disable_irq();
	rx_event_head = 0U;
	rx_event_tail = 0U;
	rx_event_overflow = false;
	if ((primask & 1U) == 0U)
		__enable_irq();
}

bool push_rx_event(RxEventType type, uint8_t byte = 0U)
{
	// 用单调递增的16位下标判断满/空，访问数组时再用mask折回环形范围。
	const uint16_t head = rx_event_head;
	if (static_cast<uint16_t>(head - rx_event_tail) >= rx_event_capacity) {
		rx_event_overflow = true;
		return false;
	}

	rx_events[head & rx_event_mask] = { type, byte };
	__DMB();
	rx_event_head = static_cast<uint16_t>(head + 1U);
	return true;
}

bool pop_rx_event(RxEvent &event)
{
	const uint16_t tail = rx_event_tail;
	if (tail == rx_event_head)
		return false;

	__DMB();
	event = rx_events[tail & rx_event_mask];
	rx_event_tail = static_cast<uint16_t>(tail + 1U);
	return true;
}

void clear_usart_rx_status()
{
	// GD32要求按“读STAT0再读DATA”的顺序清IDLE及部分接收状态，随后清错误位。
	(void)USART_STAT0(MODBUS_USART0_ADDR);
	(void)USART_DATA(MODBUS_USART0_ADDR);
	USART_STAT0(MODBUS_USART0_ADDR) &=
		~(USART_STAT0_PERR | USART_STAT0_FERR | USART_STAT0_NERR |
		  USART_STAT0_ORERR);
}

void reset_rx_dma()
{
	// 非循环DMA每次IDLE或填满后都从缓冲区开头重新装载256字节。
	dma_channel_disable(DMA1, DMA_CH2);
	dma_flag_clear(DMA1, DMA_CH2,
		       DMA_FLAG_FEE | DMA_FLAG_SDE | DMA_FLAG_TAE |
			       DMA_FLAG_HTF | DMA_FLAG_FTF);
	dma_memory_address_config(
		DMA1, DMA_CH2, DMA_MEMORY_0,
		reinterpret_cast<uint32_t>(rx_dma_buffer.data()));
	dma_transfer_number_config(DMA1, DMA_CH2, rx_dma_capacity);
	rx_dma_consumed = 0U;
}

bool consume_rx_dma()
{
	if (!rx_dma_enabled)
		return false;

	const uint32_t remaining = dma_transfer_number_get(DMA1, DMA_CH2);
	if (remaining > rx_dma_capacity)
		return false;

	const uint16_t received =
		static_cast<uint16_t>(rx_dma_capacity - remaining);
	if (received <= rx_dma_consumed)
		return false;

	__DMB();
	for (; rx_dma_consumed < received; ++rx_dma_consumed) {
		/*
		 * ASCII配置是7E1，GD32用8位字长承载7个数据位加1个校验位。DMA直接读DATA时
		 * 第8位可能带着校验位，所以ASCII入栈前只保留低7位；RTU 8E1必须保留8位。
		 */
		const uint8_t byte = ModbusConfig::mode == ModbusSerialMode::ascii ?
					     rx_dma_buffer[rx_dma_consumed] & 0x7FU :
					     rx_dma_buffer[rx_dma_consumed];
		if (!push_rx_event(RxEventType::byte,
				   byte))
			break;
	}
	return true;
}

void rearm_rx_dma()
{
	reset_rx_dma();
	if (rx_dma_enabled)
		dma_channel_enable(DMA1, DMA_CH2);
}

void configure_rx_dma()
{
	dma_single_data_parameter_struct dma_config{};

	rcu_periph_clock_enable(RCU_DMA1);
	dma_deinit(DMA1, DMA_CH2);
	dma_config.direction = DMA_PERIPH_TO_MEMORY;
	dma_config.memory0_addr =
		reinterpret_cast<uint32_t>(rx_dma_buffer.data());
	dma_config.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
	dma_config.number = rx_dma_capacity;
	dma_config.periph_addr =
		reinterpret_cast<uint32_t>(&USART_DATA(MODBUS_USART0_ADDR));
	dma_config.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
	dma_config.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
	dma_config.priority = DMA_PRIORITY_ULTRA_HIGH;
	dma_single_data_mode_init(DMA1, DMA_CH2, &dma_config);
	// 不使用循环模式，避免软件正在消费时DMA悄悄绕回开头覆盖旧字节。
	dma_circulation_disable(DMA1, DMA_CH2);
	// GD32F470 USART0_RX的官方映射是DMA1 Channel2/Subperipheral4。
	dma_channel_subperipheral_select(DMA1, DMA_CH2, DMA_SUBPERI4);
	rx_dma_consumed = 0U;
	rx_dma_enabled = false;
}

constexpr eMBParity configured_parity()
{
	if constexpr (ModbusConfig::parity == ModbusSerialParity::odd)
		return MB_PAR_ODD;
	if constexpr (ModbusConfig::parity == ModbusSerialParity::even)
		return MB_PAR_EVEN;
	return MB_PAR_NONE;
}
}

extern "C" BOOL xMBPortSerialInit(UCHAR port, ULONG baudrate, UCHAR data_bits,
				   eMBParity parity)
{
	// FreeModbus传下来的参数必须和编译期配置一致，不一致就让初始化明确失败。
	const UCHAR expected_data_bits =
		ModbusConfig::mode == ModbusSerialMode::ascii ? 7U : 8U;
	if (port != ModbusConfig::serial_port ||
	    baudrate != ModbusConfig::baudrate || data_bits != expected_data_bits ||
	    parity != configured_parity())
		return FALSE;

	MODBUS_DIRECTION::init();
	MODBUS_DIRECTION::receive();
	reset_rx_events();
	MODBUS_USART0::init();
	MODBUS_USART0::enable_it(ModbusConfig::usart_irq_priority,
				 ModbusConfig::usart_irq_sub_priority);
	configure_rx_dma();
	usart_interrupt_disable(MODBUS_USART0_ADDR, USART_INT_RBNE);
	usart_interrupt_disable(MODBUS_USART0_ADDR, USART_INT_PERR);
	usart_interrupt_disable(MODBUS_USART0_ADDR, USART_INT_ERR);
	usart_interrupt_disable(MODBUS_USART0_ADDR, USART_INT_IDLE);
	usart_interrupt_disable(MODBUS_USART0_ADDR, USART_INT_TBE);
	usart_dma_receive_config(MODBUS_USART0_ADDR,
				 USART_RECEIVE_DMA_DISABLE);
	return TRUE;
}

extern "C" void vMBPortSerialEnable(BOOL rx_enable, BOOL tx_enable)
{
	/*
	 * FreeModbus用这个函数在收发之间切换。即使当前硬件是全双工，也保留方向接口，
	 * 以后接RS485时只需实现MODBUS_DIRECTION。关闭发送前必须等TC，而不只是TBE；
	 * TBE只代表数据寄存器空了，最后一个字节可能还在线上发送。
	 */
	if (transmitter_enabled && tx_enable != TRUE) {
		while (usart_flag_get(MODBUS_USART0_ADDR, USART_FLAG_TC) == RESET)
			;
		transmitter_enabled = false;
	}

	if (rx_enable == TRUE) {
		// 每次重新进入接收都从干净DMA缓冲开始，不能把刚发送期间的杂字节当新请求。
		MODBUS_DIRECTION::receive();
	}
	if (tx_enable == TRUE) {
		MODBUS_DIRECTION::transmit();
		transmitter_enabled = true;
	}

	if (rx_enable == TRUE) {
		clear_usart_rx_status();
		rx_dma_enabled = true;
		rearm_rx_dma();
		usart_dma_receive_config(MODBUS_USART0_ADDR,
					 USART_RECEIVE_DMA_ENABLE);
		usart_interrupt_enable(MODBUS_USART0_ADDR, USART_INT_PERR);
		usart_interrupt_enable(MODBUS_USART0_ADDR, USART_INT_ERR);
		usart_interrupt_enable(MODBUS_USART0_ADDR, USART_INT_IDLE);
	} else {
		usart_interrupt_disable(MODBUS_USART0_ADDR, USART_INT_IDLE);
		usart_interrupt_disable(MODBUS_USART0_ADDR, USART_INT_PERR);
		usart_interrupt_disable(MODBUS_USART0_ADDR, USART_INT_ERR);
		usart_dma_receive_config(MODBUS_USART0_ADDR,
					 USART_RECEIVE_DMA_DISABLE);
		rx_dma_enabled = false;
		dma_channel_disable(DMA1, DMA_CH2);
		reset_rx_events();
	}

	if (tx_enable == TRUE)
		usart_interrupt_enable(MODBUS_USART0_ADDR, USART_INT_TBE);
	else
		usart_interrupt_disable(MODBUS_USART0_ADDR, USART_INT_TBE);
}

extern "C" BOOL xMBPortSerialGetByte(CHAR *byte)
{
	if (byte == nullptr)
		return FALSE;

	*byte = static_cast<CHAR>(received_byte);
	return TRUE;
}

extern "C" BOOL xMBPortSerialPutByte(CHAR byte)
{
	usart_data_transmit(MODBUS_USART0_ADDR, static_cast<uint8_t>(byte));
	return TRUE;
}

extern "C" void xMBPortSerialClose(void)
{
	vMBPortSerialEnable(FALSE, FALSE);
}

extern "C" void vMBPortClose(void)
{
	xMBPortSerialClose();
	MODBUS_TIMER::stop();
	reset_rx_events();
}

extern "C" BOOL xMBPortTimersInit(USHORT timeout_50us)
{
	/*
	 * FreeModbus把超时按50us为单位传入。这里根据120MHz TIMER6时钟动态算PSC/ARR：
	 * RTU用它产生t3.5，ASCII用它产生默认字符超时。定时器只在需要时启动，不常驻跑。
	 */
	if (timeout_50us == 0U)
		return FALSE;

	const uint64_t total_cycles =
		(static_cast<uint64_t>(ModbusConfig::timer_clock_hz) * timeout_50us) /
		20000U;
	uint32_t divider = static_cast<uint32_t>((total_cycles + 65535U) / 65536U);
	if (divider == 0U)
		divider = 1U;
	if (divider > 65536U)
		return FALSE;
	const uint32_t period =
		static_cast<uint32_t>((total_cycles + divider - 1U) / divider);
	if (period == 0U || period > 65536U)
		return FALSE;

	MODBUS_TIMER::init(static_cast<uint16_t>(divider - 1U), period - 1U);
	MODBUS_TIMER::stop();
	timer_update_source_config(MODBUS_TIMER_ADDR, TIMER_UPDATE_SRC_REGULAR);
	timer_interrupt_flag_clear(MODBUS_TIMER_ADDR, TIMER_INT_FLAG_UP);
	timer_interrupt_enable(MODBUS_TIMER_ADDR, TIMER_INT_UP);
	nvic_irq_enable(TIMER6_IRQn, ModbusConfig::timer_irq_priority,
			ModbusConfig::timer_irq_sub_priority);
	return TRUE;
}

extern "C" void vMBPortTimersEnable(void)
{
	// 每收到一个字节都把帧间定时器从0重新开始计数。
	if (replaying_rx_event)
		return;

	MODBUS_TIMER::stop();
	MODBUS_TIMER::set_counter(0U);
	timer_interrupt_flag_clear(MODBUS_TIMER_ADDR, TIMER_INT_FLAG_UP);
	NVIC_ClearPendingIRQ(TIMER6_IRQn);
	MODBUS_TIMER::start();
}

extern "C" void vMBPortTimersDisable(void)
{
	MODBUS_TIMER::stop();
	MODBUS_TIMER::set_counter(0U);
	timer_interrupt_flag_clear(MODBUS_TIMER_ADDR, TIMER_INT_FLAG_UP);
}

extern "C" void xMBPortTimersClose(void)
{
	MODBUS_TIMER::stop();
}

extern "C" void vMBPortPoll(void)
{
	/*
	 * 主循环先检查DMA计数，这是“不依赖USART IRQ也能收完整帧”的兜底。消费DMA期间
	 * 短暂屏蔽中断，避免USART0/TIMER6同时向同一个事件队列写入。
	 */
	const uint32_t primask = __get_PRIMASK();
	__disable_irq();
	const bool bytes_received = consume_rx_dma();
	if (rx_dma_consumed == rx_dma_capacity)
		rearm_rx_dma();
	if ((primask & 1U) == 0U)
		__enable_irq();
	if (bytes_received)
		vMBPortTimersEnable();

	const uint32_t status = USART_STAT0(MODBUS_USART0_ADDR);
	if ((USART_CTL0(MODBUS_USART0_ADDR) & USART_CTL0_TBEIE) != 0U &&
	    (status & USART_STAT0_TBE) != 0U)
		USART0_IRQHandler();

	if (rx_event_overflow) {
		// 丢过事件后帧边界已经不可信，清队列并用一次超时让FreeModbus放弃当前残帧。
		reset_rx_events();
		replaying_rx_event = true;
		if (pxMBPortCBTimerExpired != nullptr)
			(void)pxMBPortCBTimerExpired();
		replaying_rx_event = false;
		return;
	}

	RxEvent event{};
	while (pop_rx_event(event)) {
		/*
		 * 字节和timeout必须严格按发生顺序重放。不能把timeout单独放一个bool，否则主循环
		 * 忙时可能先处理后一帧字节，再处理前一帧结束，最终把两帧粘在一起。
		 */
		BOOL poll_needed = FALSE;
		replaying_rx_event = true;
		if (event.type == RxEventType::byte) {
			received_byte = event.byte;
			if (pxMBFrameCBByteReceived != nullptr)
				poll_needed = pxMBFrameCBByteReceived();
		} else if (pxMBPortCBTimerExpired != nullptr) {
			poll_needed = pxMBPortCBTimerExpired();
		}
		replaying_rx_event = false;

		if (poll_needed == TRUE || event.type == RxEventType::timeout)
			break;
	}
}

extern "C" void vMBPortTimersDelay(USHORT timeout_ms)
{
	const uint32_t cycles_per_ms = SystemCoreClock / 1000U / 4U;
	for (USHORT ms = 0; ms < timeout_ms; ++ms) {
		uint32_t cycles = cycles_per_ms;
		while (cycles-- > 0U)
			__asm volatile("nop");
	}
}

extern "C" void USART0_IRQHandler(void)
{
	const uint32_t status = USART_STAT0(MODBUS_USART0_ADDR);
	if ((status & USART_STAT0_IDLEF) != 0U &&
	    (USART_CTL0(MODBUS_USART0_ADDR) & USART_CTL0_IDLEIE) != 0U) {
		// IDLE约一个字符时间就会到，这里只收割DMA；真正RTU帧结束仍由TIMER6 t3.5判断。
		clear_usart_rx_status();
		dma_channel_disable(DMA1, DMA_CH2);
		const bool bytes_received = consume_rx_dma();
		rearm_rx_dma();
		if (bytes_received)
			vMBPortTimersEnable();
	} else if ((status & (USART_STAT0_PERR | USART_STAT0_FERR |
			      USART_STAT0_NERR | USART_STAT0_ORERR)) != 0U) {
		clear_usart_rx_status();
	}

	if (usart_interrupt_flag_get(MODBUS_USART0_ADDR, USART_INT_FLAG_TBE) == SET &&
	    pxMBFrameCBTransmitterEmpty != nullptr)
		(void)pxMBFrameCBTransmitterEmpty();
}

extern "C" void TIMER6_IRQHandler(void)
{
	if (timer_interrupt_flag_get(MODBUS_TIMER_ADDR, TIMER_INT_FLAG_UP) != SET)
		return;

	// timeout也进入同一个事件队列，保证它排在此前收到的所有字节后面。
	vMBPortTimersDisable();
	if (!push_rx_event(RxEventType::timeout))
		rx_event_overflow = true;
}
