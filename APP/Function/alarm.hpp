#pragma once
// 告警记录管理 — 最近10条, 环形存储, 时间倒序, Flash 持久化

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <ctime>
#include "../Driver/flash_param.hpp"

namespace Alarm
{

constexpr int MAX_RECORDS = 10;

// 告警记录在参数区 Flash 中的偏移 (紧接 DeviceParams)
constexpr uint32_t ALARM_FLASH_OFFSET =
	64; // sizeof(DeviceParams) 约 52 字节, 64 对齐

struct Record {
	uint32_t timestamp;
	uint8_t channel;
	float threshold;
	float actual_value;
	bool valid;
};

inline Record g_records[MAX_RECORDS];
inline int g_record_count = 0;
inline bool g_active = false;

inline void init()
{
	for (auto &r : g_records)
		r.valid = false;
	g_record_count = 0;
	g_active = false;
}

inline void add(uint32_t timestamp, uint8_t channel, float threshold,
		float actual)
{
	for (int i = MAX_RECORDS - 1; i > 0; --i)
		g_records[i] = g_records[i - 1];
	g_records[0].timestamp = timestamp;
	g_records[0].channel = channel;
	g_records[0].threshold = threshold;
	g_records[0].actual_value = actual;
	g_records[0].valid = true;
	if (g_record_count < MAX_RECORDS)
		++g_record_count;
}

inline void clear()
{
	for (auto &r : g_records)
		r.valid = false;
	g_record_count = 0;
}

/// 保存告警到 Flash (追加在参数区之后, 不擦除参数)
inline void save_to_flash()
{
	// 写入记录数 + 所有记录
	FlashParam::write(ALARM_FLASH_OFFSET, &g_record_count,
			  sizeof(g_record_count));
	FlashParam::write(ALARM_FLASH_OFFSET + 4, g_records, sizeof(g_records));
}

/// 从 Flash 加载告警
inline void load_from_flash()
{
	FlashParam::read(ALARM_FLASH_OFFSET, &g_record_count,
			 sizeof(g_record_count));
	if (g_record_count > MAX_RECORDS)
		g_record_count = 0;
	FlashParam::read(ALARM_FLASH_OFFSET + 4, g_records, sizeof(g_records));
}

inline int format_record(const Record &rec, char *buf, int buf_size)
{
	time_t t = rec.timestamp;
	struct tm *tm_info = gmtime(&t);
	return snprintf(
		buf, buf_size,
		"%04d-%02d-%02d %02d:%02d:%02d | CH%d | %.2f | %.2f\r\n",
		tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
		tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, rec.channel,
		rec.threshold, rec.actual_value);
}

} // namespace Alarm
