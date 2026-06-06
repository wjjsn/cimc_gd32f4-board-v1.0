#pragma once

#include <cstdint>
#include <array>

namespace Protocol
{

/// 帧解析状态
enum class Status : uint8_t {
	idle, // 暂无帧
	frame_ready, // 完整帧已提取
	incomplete, // 帧不完整,继续收
	header_not_found, // 流中未找到 A5B6
	crc_error, // CRC 校验失败
	length_error, // 报文长度字段与实际内容不匹配
	invalid_hex, // 含非法十六进制字符
};

/// 解析后的二进制帧
struct Frame {
	std::array<uint8_t, 256> data{};
	uint16_t size = 0;
};

/**
	 * @brief 协议帧解析器 (模板参数化 ringbuffer 类型)
	 *
	 * @tparam RingBuffer  需提供 static uint32_t get_used() / static bool read_byte(uint8_t*)
	 *
	 * 职责:
	 *  1. 从 ringbuffer 拉取 ASCII 十六进制字符流
	 *  2. 识别帧边界 A5B6 ... B6A5
	 *  3. ASCII hex → 二进制转换
	 *  4. 长度字段校验
	 *  5. CRC-16-Modbus 校验
	 *
	 * 通信规约: 所有协议帧按十六进制结构组帧后,以 ASCII 字符串形式在串口收发。
	 * 例如帧头 0xA5B6 实际发送字符 'A','5','B','6' (0x41,0x35,0x42,0x36)。
	 */
template <typename RingBuffer> class Parser {
    public:
	void init()
	{
		state_ = State::SEEKING_HEADER;
		ascii_pos_ = 0;
	}

	Status poll(Frame &out_frame);

	/// 二进制帧 → ASCII 十六进制字符串 (不含 '\0')
	static void frame_to_ascii(const uint8_t *binary, uint16_t binary_size,
				   char *ascii_out, uint16_t &ascii_size);

    private:
	enum State : uint8_t {
		SEEKING_HEADER, // 寻找 "A5B6"
		COLLECTING_TAIL, // 已找到帧头,等待 "B6A5" 帧尾
	};

	State state_ = State::SEEKING_HEADER;
	char ascii_buf_
		[1024]{}; // ASCII 字符缓冲 (容纳最大帧: 帧头4+帧尾4+255*2 内容 ≈ 518)
	uint16_t ascii_pos_ = 0;

	/// 从 ringbuffer 读一个字节
	static bool rb_read(uint8_t &byte)
	{
		if (RingBuffer::get_used() == 0)
			return false;
		return RingBuffer::read_byte(&byte);
	}

	/// 检查是否为合法十六进制字符 [0-9A-Fa-f]
	static bool is_hex(char c);

	/// 两个 ASCII hex 字符 → 一个字节
	static bool hex_pair_to_byte(char high, char low, uint8_t &out);

	/// 追加一个 ASCII 字符到内部缓冲 (超过容量则重置)
	bool push_ascii(char c);

	/// 对 ascii_buf_[0..ascii_pos_) 做十六进制→二进制转换并 CRC 校验
	Status decode_and_check(Frame &out_frame);
};

// ======================== 便捷别名 ========================

/**
	 * @brief 应答帧构建器: 提供常用协议的组帧方法
	 *
	 * 使用示例:
	 *   Response::build_ok(device_id, command_word, out_data, out_size);
	 */
namespace Response
{

/// 帧类型常量
constexpr uint8_t FTYPE_CMD = 0x01; // 上位机 → 设备
constexpr uint8_t FTYPE_RSP = 0x02; // 设备 → 上位机 (应答)
constexpr uint8_t FTYPE_HB = 0x05; // 心跳
constexpr uint8_t FTYPE_ERR = 0xFF; // 错误应答

/// 协议版本
constexpr uint8_t PROTOCOL_VERSION = 0x02;

/// 帧头帧尾
constexpr uint16_t FRAME_HEAD = 0xA5B6;
constexpr uint16_t FRAME_TAIL = 0xB6A5;

/// 构建 OK 应答帧 (内容 = 0xFF)
/// @return 二进制帧字节数
uint16_t build_ok(uint16_t device_id, uint16_t cmd_word, uint8_t *out_data,
		  uint16_t out_capacity);

/// 构建错误应答帧 (命令字 = 0xEEEE)
uint16_t build_error(uint16_t device_id, uint8_t *out_data,
		     uint16_t out_capacity);

/// 构建心跳帧 (命令字 = 0x8888)
uint16_t build_heartbeat(uint16_t device_id, uint8_t *out_data,
			 uint16_t out_capacity);

/// 构建含内容的应答帧 (content_size 为 payload 字节数)
uint16_t build_response(uint16_t device_id, uint16_t cmd_word,
			const uint8_t *content, uint8_t content_size,
			uint8_t *out_data, uint16_t out_capacity);

} // namespace Response

} // namespace Protocol

// ======================== 模板实现 ========================
#include "protocol_impl.hpp"
