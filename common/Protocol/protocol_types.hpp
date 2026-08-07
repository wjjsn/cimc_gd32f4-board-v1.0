#pragma once
// 协议公共类型 — Frame 结构体、解析状态枚举

#include <cstdint>
#include <array>

/// 协议帧解析状态
enum class ProtocolStatus : uint8_t {
	idle, // 暂无帧
	frame_ready, // 完整帧已提取
	incomplete, // 帧不完整,继续收
	header_not_found, // 流中未找到 A5B6
	crc_error, // CRC 校验失败
	length_error, // 报文长度字段与实际内容不匹配
	invalid_hex, // 含非法十六进制字符
};

/// 解析后的二进制帧 (最大 256 字节)
struct ProtocolFrame {
	std::array<uint8_t, 256> data{};
	uint16_t size = 0;
};
