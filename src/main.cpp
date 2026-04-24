#include <iostream>
#include "../include/Monitor.h"
#include <thread>
#include <chrono>
#include <locale>

int main() {
    SysMonitor monitor;
   std::wcout.imbue(std::locale("chs"));
    std::cout << "--- " << "System Real-time Monitor (Press Ctrl+C to stop)" << " ---" << std::endl;
    std::wcout << monitor.GetSystemInfo() << std::endl;
    while (true) {
        double usage = monitor.GetCpuUsage();
        std::cout << "\rCPU Usage: " << usage << "%    " << std::flush;

        // 每隔 1 秒刷新一次
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    return 0;
}