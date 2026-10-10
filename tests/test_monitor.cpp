// Unit tests for the monitor core.
//
// Deliberately dependency-free: a single CHECK macro and an exit code, so the
// tests build anywhere the library does. Run through CTest (see CMakeLists.txt)
// or invoke the binary directly.

#include "../include/Monitor.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace {

int g_checks   = 0;
int g_failures = 0;

#define CHECK(condition)                                                  \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(condition)) {                                               \
            ++g_failures;                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__,           \
                        #condition);                                      \
        }                                                                 \
    } while (false)

void TestCpuSamplerBaseline() {
    sysmon::CpuSampler sampler;

    // The very first sample only records a baseline: there is nothing to
    // subtract from yet, so the result must be empty rather than a bogus 0.
    CHECK(!sampler.Sample().has_value());
}

void TestCpuSamplerRange() {
    sysmon::CpuSampler sampler;
    sampler.Sample();  // establish baseline

    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    const std::optional<double> value = sampler.Sample();
    CHECK(value.has_value());
    if (value.has_value()) {
        // Tolerance is inherent: a busy CI box can pin a core, but a valid
        // percentage can never leave [0, 100].
        CHECK(*value >= 0.0);
        CHECK(*value <= 100.0);
    }
}

void TestCpuSamplerReset() {
    sysmon::CpuSampler sampler;
    sampler.Sample();
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    CHECK(sampler.Sample().has_value());

    sampler.Reset();
    CHECK(!sampler.Sample().has_value());  // back to needing a baseline
}

void TestSystemInfo() {
    sysmon::SystemInfo info;
    std::wstring error;
    CHECK(sysmon::QuerySystemInfo(info, &error));

    CHECK(!info.hostname.empty());
    CHECK(!info.osVersion.empty());
    CHECK(!info.cpuModel.empty());
    CHECK(info.logicalCores >= 1);
    CHECK(info.totalMemoryBytes > 0);
    // Physical cores may be reported as 0 when the API is unavailable, but
    // must never exceed the logical count.
    CHECK(info.physicalCores <= info.logicalCores);
}

void TestMemoryStatus() {
    sysmon::MemoryStatus memory;
    std::wstring error;
    CHECK(sysmon::QueryMemoryStatus(memory, &error));

    CHECK(memory.totalBytes > 0);
    CHECK(memory.availableBytes <= memory.totalBytes);
    CHECK(memory.usedBytes <= memory.totalBytes);
    CHECK(memory.usagePercent >= 0.0);
    CHECK(memory.usagePercent <= 100.0);
}

void TestDisks() {
    std::vector<sysmon::DiskStatus> disks;
    std::wstring error;
    CHECK(sysmon::QueryDisks(disks, &error));
    CHECK(!disks.empty());

    for (const sysmon::DiskStatus& disk : disks) {
        CHECK(!disk.mountPoint.empty());
        CHECK(disk.totalBytes > 0);
        CHECK(disk.freeBytes <= disk.totalBytes);
        CHECK(disk.usagePercent >= 0.0);
        CHECK(disk.usagePercent <= 100.0);
    }
}

}  // namespace

int main() {
    TestCpuSamplerBaseline();
    TestCpuSamplerRange();
    TestCpuSamplerReset();
    TestSystemInfo();
    TestMemoryStatus();
    TestDisks();

    std::printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
