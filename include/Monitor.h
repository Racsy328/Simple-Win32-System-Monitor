#pragma once
#include <windows.h>
#include <string>

class SysMonitor {
public:
    SysMonitor();
    double GetCpuUsage(); // 获取CPU占用率
    std::wstring GetSystemInfo(); // 获取系统基本信息
private:
    FILETIME preIdleTime;
    FILETIME preKernelTime;
    FILETIME preUserTime;
};
