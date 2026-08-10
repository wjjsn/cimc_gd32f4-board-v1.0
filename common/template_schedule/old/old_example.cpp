#include <iostream>
#include "schedule.hpp"
#include <tuple>
#include <chrono>
#include <thread>


static constexpr TaskConfig myConfigs[] = {
    { 1000, [](){std::cout << "task1 triggered!\n"; } },
    { 5000, [](){std::cout << "task2 triggered!\n"; } }
};

using schedule = StaticTimerManager<
    []() -> uint64_t { 
        static auto now=0;
        now++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 模拟时间流逝
        return now; 
    }, 
    myConfigs,
    std::size(myConfigs)
>;


int main(int argc, char **argv) {
    while (true) {
        schedule::poll(); // 轮询定时器
    }
    return 0;
}
