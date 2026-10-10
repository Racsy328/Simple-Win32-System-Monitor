#pragma once

// SystemMonitor - a lightweight, dependency-free system monitor for Windows.
//
// Everything here is implemented on top of the Win32 SDK only: no third-party
// libraries, no COM, no WMI. The public surface is intentionally small and
// every query reports failure through a return value instead of throwing.

#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sysmon {

/// Version reported by `--version` and embedded in JSON output.
inline constexpr const wchar_t* kVersion = L"2.0.0";

/// Static (or slowly changing) properties of the machine.
struct SystemInfo {
    std::wstring  hostname;          ///< NetBIOS/DNS host name
    std::wstring  osVersion;         ///< e.g. "Windows 11 Pro (Build 26100)"
    std::wstring  cpuModel;          ///< e.g. "12th Gen Intel(R) Core(TM) i7-12700H"
    unsigned      logicalCores  = 0; ///< schedulable logical processors
    unsigned      physicalCores = 0; ///< physical cores on the die(s)
    std::uint64_t totalMemoryBytes = 0;
    std::uint64_t uptimeSeconds    = 0;
};

/// Point-in-time physical memory statistics.
struct MemoryStatus {
    std::uint64_t totalBytes     = 0;
    std::uint64_t availableBytes = 0;
    std::uint64_t usedBytes      = 0;
    double        usagePercent   = 0.0;
};

/// Capacity of a single mounted volume that is currently ready.
struct DiskStatus {
    std::wstring  mountPoint;        ///< e.g. L"C:\\"
    std::uint64_t totalBytes   = 0;
    std::uint64_t freeBytes    = 0;
    double        usagePercent = 0.0;
};

/// Fills `out` with static system information.
/// Returns false on failure; `errorOut` (when non-null) then receives a
/// human-readable description of what went wrong.
bool QuerySystemInfo(SystemInfo& out, std::wstring* errorOut = nullptr);

/// Fills `out` with the current physical memory usage.
bool QueryMemoryStatus(MemoryStatus& out, std::wstring* errorOut = nullptr);

/// Appends one entry per ready fixed or removable volume to `out`.
/// Returns false when no volume could be queried at all.
bool QueryDisks(std::vector<DiskStatus>& out, std::wstring* errorOut = nullptr);

/// Stateful CPU usage sampler.
///
/// A CPU usage figure is a *delta* between two samples, so the very first call
/// to Sample() only records a baseline and returns std::nullopt. Every call
/// after that returns a percentage in [0, 100], or std::nullopt when the
/// underlying Win32 call fails or the sample window contains no CPU time.
class CpuSampler {
public:
    CpuSampler() = default;

    std::optional<double> Sample();

    /// Drops the stored baseline; the next Sample() behaves like the first.
    void Reset() noexcept;

private:
    bool          hasBaseline_ = false;
    std::uint64_t prevIdle_    = 0;
    std::uint64_t prevKernel_  = 0;
    std::uint64_t prevUser_    = 0;
};

}  // namespace sysmon
