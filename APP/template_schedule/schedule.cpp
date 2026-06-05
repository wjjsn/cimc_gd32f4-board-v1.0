#include <iostream>
#include "schedule.hpp"
#include <tuple>
#include <chrono>
#include <thread>

using schedule = StaticTimerManager<
    []() -> uint64_t { 
        static auto now=0;
        now++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // 模拟时间流逝
        return now; 
    }, 
    {1000, []() { std::cout << "1000ms 触发一次 triggered!\n"; }}, // 每隔 1000ms 触发一次
    {2000, []() { std::cout << "2000ms 触发一次 triggered!\n"; }}  // 每隔 2000ms 触发一次
>;


int main(int argc, char **argv) {
    while (true) {
        schedule::poll(); // 轮询定时器
    }
    return 0;
}
