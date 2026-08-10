#include <cstdint>

using CallbackFunc = void (*)();

/*
 * 模板参数说明：
 * - GPIO: 传入的 GPIO 驱动类（需提供 init() 和 read() 方法）
 * - TrigState: 触发电平（false: 低电平触发 / 有效，true: 高电平触发 / 有效）
 * - HoldTask: 长按回调函数
 * - ClickTasks: 连击回调函数包（依次为 单击、双击、三击...）
 */
template <typename GPIO, bool TrigState, CallbackFunc HoldTask,
	  CallbackFunc... ClickTasks>
struct StaticKey {
    private:
	// 编译期自动计算注册的最高连击次数
	static constexpr std::size_t MAX_CLICKS = sizeof...(ClickTasks);

	// 静态状态变量
	inline static bool stateRealTime = !TrigState;
	inline static bool stateLastTime = !TrigState;
	inline static uint32_t clickCountRealTime = 0;
	inline static uint32_t clickCountLastTime = 0;
	inline static uint32_t holdCount = 0;
	inline static bool holding = false;

    public:
	// 1. 初始化接口
	static void init()
	{
		GPIO::init();
		stateRealTime = GPIO::read();
		stateLastTime = stateRealTime;
	}

	// 2. 检测长按状态
	static void detect_key_hold()
	{
		if (holdCount >= 60) {
			holdCount = 60;
			holding = true;
			if (HoldTask) {
				HoldTask();
			}
		}
	}

	// 3. 按键状态扫描（建议在定时器中断中调用，如 10ms 或 20ms）
	static void detect_key_click()
	{
		stateLastTime = stateRealTime;
		stateRealTime = GPIO::read();

		// 检测释放边缘（从触发电平变为非触发电平，计为一次点击完成）
		if (stateRealTime == (!TrigState) &&
		    stateLastTime == TrigState) {
			clickCountRealTime++;
		}

		// 持续处于触发状态，累加长按计数
		if (stateRealTime == TrigState && stateLastTime == TrigState) {
			holdCount++;
		}

		// 持续处于释放状态，清空长按计数
		if (stateRealTime == (!TrigState) &&
		    stateLastTime == (!TrigState)) {
			holdCount = 0;
		}
	}

	// 4. 处理点击事件（在主循环中调用）
	static void cope_click_data()
	{
		// 长按判定 (从 detect_key_click 移到这里, 回调在主循环上下文执行)
		detect_key_hold();

		// 状态还在改变，或者没有点击，直接返回（消抖或等待连续点击结束）
		if (clickCountRealTime != clickCountLastTime ||
		    clickCountRealTime == 0) {
			clickCountLastTime = clickCountRealTime;
			return;
		}

		// 计数稳定且不为 0，说明连击结束，开始处理
		if (clickCountRealTime == clickCountLastTime &&
		    clickCountRealTime != 0) {
			std::size_t clickIndex = clickCountRealTime - 1;

			// 将编译期参数包直接初始化为局部静态只读数组
			static constexpr CallbackFunc tasks[] = {
				ClickTasks...
			};

			// O(1) 数组直达，安全检查边界后直接调用
			if (clickIndex < MAX_CLICKS) {
				if (clickIndex == 0 && holding) {
					holding = false;
				} else if (tasks[clickIndex]) {
					tasks[clickIndex]();
				}
			}

			// 计数器清零
			clickCountRealTime = clickCountLastTime = 0;
		}
	}
};