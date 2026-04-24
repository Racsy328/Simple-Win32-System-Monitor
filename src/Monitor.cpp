#include "../include/Monitor.h"
std::wstring SysMonitor::GetSystemInfo() {
    return L"Windows System Performance Agent v1.0";
}
SysMonitor::SysMonitor() {
    GetSystemTimes(&preIdleTime, &preKernelTime, &preUserTime);
}

double SysMonitor::GetCpuUsage() {
    FILETIME idleTime, kernelTime, userTime;
    if (GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        // ¼ÆËã²îÖµÂß¼­ (¼òÐ´)
        auto CompareFileTime = [](FILETIME a, FILETIME b) -> ULONGLONG {
            return ((ULONGLONG)a.dwHighDateTime << 32 | a.dwLowDateTime) -
                ((ULONGLONG)b.dwHighDateTime << 32 | b.dwLowDateTime);
            };
        ULONGLONG idle = CompareFileTime(idleTime, preIdleTime);
        ULONGLONG kernel = CompareFileTime(kernelTime, preKernelTime);
        ULONGLONG user = CompareFileTime(userTime, preUserTime);
        preIdleTime = idleTime; preKernelTime = kernelTime; preUserTime = userTime;
        return (double)(kernel + user - idle) / (kernel + user) * 100.0;
    }
    return 0.0;
}