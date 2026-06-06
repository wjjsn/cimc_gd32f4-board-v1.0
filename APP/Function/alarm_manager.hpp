#pragma once
// 告警记录管理 — 最近10条, 环形存储, 时间倒序, Flash 持久化

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <ctime>
#include "../Driver/flash_param.hpp"

class AlarmManager {
    public:
	static constexpr int MAX_RECORDS = 10;

	// 告警记录在参数区 Flash 中的偏移 (紧接 DeviceParams)
	static constexpr uint32_t ALARM_FLASH_OFFSET = 64;

	struct Record {
		uint32_t timestamp;
		uint8_t channel;
		float threshold;
		float actual_value;
		bool valid;
	};

	void init()
	{
		for (auto &r : records_)
			r.valid = false;
		record_count_ = 0;
		active_ = false;
	}

	void add(uint32_t timestamp, uint8_t channel, float threshold,
		 float actual)
	{
		for (int i = MAX_RECORDS - 1; i > 0; --i)
			records_[i] = records_[i - 1];
		records_[0].timestamp = timestamp;
		records_[0].channel = channel;
		records_[0].threshold = threshold;
		records_[0].actual_value = actual;
		records_[0].valid = true;
		if (record_count_ < MAX_RECORDS)
			++record_count_;
	}

	void clear()
	{
		for (auto &r : records_)
			r.valid = false;
		record_count_ = 0;
	}

	/// 保存告警到 Flash
	void save_to_flash()
	{
		flash_param_write(ALARM_FLASH_OFFSET, &record_count_,
				  sizeof(record_count_));
		flash_param_write(ALARM_FLASH_OFFSET + 4, records_,
				  sizeof(records_));
	}

	/// 从 Flash 加载告警
	void load_from_flash()
	{
		flash_param_read(ALARM_FLASH_OFFSET, &record_count_,
				 sizeof(record_count_));
		if (record_count_ > MAX_RECORDS)
			record_count_ = 0;
		flash_param_read(ALARM_FLASH_OFFSET + 4, records_,
				 sizeof(records_));
	}

	static int format_record(const Record &rec, char *buf, int buf_size)
	{
		time_t t = rec.timestamp;
		struct tm *tm_info = gmtime(&t);
		return snprintf(
			buf, buf_size,
			"%04d-%02d-%02d %02d:%02d:%02d | CH%d | %.2f | %.2f\r\n",
			tm_info->tm_year + 1900, tm_info->tm_mon + 1,
			tm_info->tm_mday, tm_info->tm_hour, tm_info->tm_min,
			tm_info->tm_sec, rec.channel, rec.threshold,
			rec.actual_value);
	}

	// ——— 公开成员 ———
	Record records_[MAX_RECORDS]{};
	int record_count_ = 0;
	bool active_ = false;
};
