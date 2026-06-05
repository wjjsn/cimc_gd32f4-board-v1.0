#include <cstdint>

// 为了代码可读性，先将确定的类型进行别名定义
using TimeProviderFunc = uint64_t(*)(); // 返回 uint64_t 的时间函数指针
using CallbackFunc = void(*)();         // 触发时调用的回调函数指针
struct TaskConfig {
    uint64_t interval;
    CallbackFunc callback;
}; // (隔多久触发一次, 回调函数)

// 模板类定义：第一个参数是时间函数，后续是至少一个 TimerTuple
template <TimeProviderFunc GetTime, TaskConfig FirstTimer, TaskConfig... RemainingTimers>
class StaticTimerManager {
private:
    // 将所有传入的元组静态组合成一个 std::tuple 数组
    // 这里的类型是完全确定的
    static constexpr std::size_t TimerCount = 1 + sizeof...(RemainingTimers);
    static constexpr TaskConfig Timers[TimerCount] = { FirstTimer, RemainingTimers... };

    // 静态内存：记录上一次触发的时间戳（初始化为 0）
    static inline uint64_t lastTriggerTimes[TimerCount] = { 0 };

    // 记录上一次处理到的位置，用于轮询公平性
    static inline std::size_t currentIndex = 0;

public:
    // 静态轮询函数：在主循环中调用此函数
    // 返回值: 本轮是否触发了至少一个任务 (true=有任务执行, false=空闲)
    static bool poll() {
        uint64_t currentTime = GetTime();
        bool fired = false;

        // 使用编译期展开或循环来处理确定的数组
        for (std::size_t i = 0; i < TimerCount; ++i) {
            // 计算实际索引：从 currentIndex 开始，跳过已处理的
            std::size_t idx = (currentIndex + i) % TimerCount;
            uint64_t interval = Timers[idx].interval;
            CallbackFunc callback = Timers[idx].callback;

            // 首次运行时初始化时间戳，避免启动时瞬间全部触发
            if (lastTriggerTimes[idx] == 0) {
                lastTriggerTimes[idx] = currentTime;
                break; // 初始化后跳出，下一次 poll 从下一个任务开始检查
            }

            // 检查是否到达时间间隔
            if (currentTime - lastTriggerTimes[idx] >= interval) {
                if (callback) {
                    callback(); // 执行回调
                }
                lastTriggerTimes[idx] = currentTime; // 更新时间

                // 一旦执行了某个任务，下一次 poll 从它的下一个任务开始检查
                currentIndex = (idx + 1) % TimerCount;
                fired = true;
            }
        }

        return fired;
    }
};