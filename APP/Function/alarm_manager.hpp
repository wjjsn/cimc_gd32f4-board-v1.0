#pragma once
// 告警记录管理 — 最近10条, 环形存储, 时间倒序, Flash 持久化

#include <cstdint>
#include <cstring>
#include <cstdio>
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

	/// 保存告警到 Flash (先备份参数区, 擦除扇区, 再恢复)
	void save_to_flash()
	{
		// 备份参数区数据 (DeviceParams 区域: offset 0 ~ ALARM_FLASH_OFFSET)
		uint8_t param_buf[ALARM_FLASH_OFFSET];
		flash_param_read(0, param_buf, sizeof(param_buf));

		// 擦除整个扇区 (4KB)
		flash_param_erase_sector();

		// 恢复参数区
		flash_param_write(0, param_buf, sizeof(param_buf));

		// 写入告警数据
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
		// 手动将 Unix 时间戳解码为年月日时分秒, UTC+8 (北京时间)
		uint32_t utc = rec.timestamp + 8U * 3600U;
		uint32_t tod = utc % 86400;
		uint8_t hr = tod / 3600;
		uint8_t mi = (tod % 3600) / 60;
		uint8_t se = tod % 60;
		uint32_t days = utc / 86400;
		uint16_t yr = 1970;
		while (true) {
			bool lp = (yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0));
			uint16_t diy = lp ? 366 : 365;
			if (days >= diy) { days -= diy; ++yr; } else break;
		}
		bool lp = (yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0));
		const uint8_t dm[] = {31, uint8_t(lp ? 29 : 28), 31, 30, 31, 30,
				     31, 31, 30, 31, 30, 31};
		uint8_t mo = 0;
		while (days >= dm[mo]) { days -= dm[mo]; ++mo; }
		uint8_t da = uint8_t(days + 1);

		return snprintf(
			buf, buf_size,
			"%04d-%02d-%02d%02d:%02d:%02d|CH%d|%.2f|%.2f",
			yr, mo + 1, da, hr, mi, se,
			rec.channel, rec.threshold, rec.actual_value);
	}

	// ——— 公开成员 ———
	Record records_[MAX_RECORDS]{};
	int record_count_ = 0;
	bool active_ = false;
};
