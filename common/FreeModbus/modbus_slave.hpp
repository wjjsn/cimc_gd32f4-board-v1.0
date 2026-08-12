#pragma once

// 初始化并启用配置选中的RTU或ASCII从站；配置非法时返回false。
bool modbus_slave_init();
// 必须在主循环中高频调用，不能放到100ms之类的低频定时任务中。
void modbus_slave_poll();
// 深度睡眠或外设时钟关闭前调用，停止协议栈和端口层。
void modbus_slave_suspend();
// 系统时钟恢复后重新初始化USART0、DMA、TIMER6和协议状态机。
bool modbus_slave_resume();
