#include "../include/Monitor.h"

#include <algorithm>
#include <cstdlib>
#include <cwchar>

namespace sysmon {
namespace {

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

std::wstring FormatWin32Error(DWORD code) {
    LPWSTR buffer = nullptr;
    const DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);

    std::wstring message =
        (length != 0 && buffer != nullptr) ? std::wstring(buffer, length)
                                           : L"unknown error";
    if (buffer != nullptr) {
        ::LocalFree(buffer);
    }

    while (!message.empty() &&
           (message.back() == L'\r' || message.back() == L'\n' ||
            message.back() == L' ')) {
        message.pop_back();
    }
    return message;
}

void SetError(std::wstring* out, std::wstring message) {
    if (out != nullptr) {
        *out = std::move(message);
    }
}

/// Reads a REG_SZ value from HKLM. Returns an empty string when absent.
std::wstring ReadMachineString(const wchar_t* subKey, const wchar_t* valueName) {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &key) !=
        ERROR_SUCCESS) {
        return {};
    }

    DWORD  type = 0;
    DWORD  bytes = 0;
    if (::RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &bytes) !=
            ERROR_SUCCESS ||
        type != REG_SZ || bytes == 0) {
        ::RegCloseKey(key);
        return {};
    }

    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    LSTATUS status = ::RegQueryValueExW(
        key, valueName, nullptr, &type,
        reinterpret_cast<LPBYTE>(value.data()), &bytes);
    ::RegCloseKey(key);

    if (status != ERROR_SUCCESS) {
        return {};
    }

    // The buffer may carry a trailing NUL that RegQueryValueExW counted in
    // `bytes`; trim it before returning.
    while (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value;
}

/// RtlGetVersion is the only reliable way to learn the real OS version on
/// Windows 8.1+, where GetVersionExW deliberately lies unless the binary is
/// manifested. It is declared here rather than pulled from winternl.h to keep
/// the include surface small; OSVERSIONINFOW is layout-compatible.
using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);

void QueryOsVersionString(std::wstring& out) {
    constexpr const wchar_t* kCurrentVersionKey =
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

    DWORD buildNumber = 0;
    bool  resolved    = false;

    if (HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll")) {
        auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
            ::GetProcAddress(ntdll, "RtlGetVersion"));
        OSVERSIONINFOW info{};
        info.dwOSVersionInfoSize = sizeof(info);
        if (rtlGetVersion != nullptr &&
            rtlGetVersion(&info) == 0 /* STATUS_SUCCESS */) {
            buildNumber = info.dwBuildNumber;
            resolved    = true;
        }
    }

    if (!resolved) {
        // Fallback for the unlikely case ntdll is unavailable. Reading the
        // registry avoids GetVersionExW, which is deprecated (C4996) and
        // deliberately inaccurate on Windows 8.1+.
        const std::wstring buildText =
            ReadMachineString(kCurrentVersionKey, L"CurrentBuildNumber");
        if (!buildText.empty()) {
            buildNumber =
                static_cast<DWORD>(std::wcstoul(buildText.c_str(), nullptr, 10));
            resolved = true;
        }
    }

    std::wstring productName =
        ReadMachineString(kCurrentVersionKey, L"ProductName");
    if (productName.empty()) {
        productName = L"Windows";
    }

    // Windows 11 still advertises itself as "Windows 10 ..." in ProductName:
    // Microsoft never updated that registry value. Correct it from the build
    // number so the reported name matches what the user actually runs.
    if (resolved && buildNumber >= 22000) {
        constexpr const wchar_t* kLegacyName = L"Windows 10";
        const std::size_t position = productName.find(kLegacyName);
        if (position != std::wstring::npos) {
            productName.replace(position, std::wcslen(kLegacyName),
                                L"Windows 11");
        }
    }

    if (!resolved) {
        out = productName;
        return;
    }

    const std::wstring displayVersion =
        ReadMachineString(kCurrentVersionKey, L"DisplayVersion");

    wchar_t build[64] = {};
    if (displayVersion.empty()) {
        ::swprintf_s(build, L" (Build %lu)",
                     static_cast<unsigned long>(buildNumber));
    } else {
        ::swprintf_s(build, L" %ls (Build %lu)", displayVersion.c_str(),
                     static_cast<unsigned long>(buildNumber));
    }
    out = productName + build;
}

void QueryCpuModel(std::wstring& out) {
    out = ReadMachineString(
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        L"ProcessorNameString");
    if (out.empty()) {
        out = L"unknown";
    } else {
        // The registry value is often space padded.
        const auto first = out.find_first_not_of(L' ');
        const auto last  = out.find_last_not_of(L' ');
        out = (first == std::wstring::npos)
                  ? std::wstring()
                  : out.substr(first, last - first + 1);
    }
}

/// Counts physical cores via GetLogicalProcessorInformationEx.
/// Returns 0 when the information cannot be obtained.
unsigned CountPhysicalCores() {
    DWORD length = 0;
    ::GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    if (length == 0) {
        return 0;
    }

    std::vector<BYTE> buffer(length);
    if (!::GetLogicalProcessorInformationEx(
            RelationProcessorCore,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(
                buffer.data()),
            &length)) {
        return 0;
    }

    unsigned cores = 0;
    DWORD    offset = 0;
    while (offset < length) {
        const auto* entry =
            reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
                buffer.data() + offset);
        if (entry->Relationship == RelationProcessorCore) {
            ++cores;
        }
        if (entry->Size == 0) {
            break;  // defensive: avoid an infinite loop on malformed data
        }
        offset += entry->Size;
    }
    return cores;
}

std::uint64_t FileTimeToTicks(const FILETIME& ft) noexcept {
    return (static_cast<std::uint64_t>(ft.dwHighDateTime) << 32) |
           static_cast<std::uint64_t>(ft.dwLowDateTime);
}

double Percentage(std::uint64_t used, std::uint64_t total) {
    if (total == 0) {
        return 0.0;
    }
    return static_cast<double>(used) / static_cast<double>(total) * 100.0;
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool QuerySystemInfo(SystemInfo& out, std::wstring* errorOut) {
    wchar_t hostname[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD   hostnameLength = MAX_COMPUTERNAME_LENGTH + 1;
    if (::GetComputerNameW(hostname, &hostnameLength)) {
        out.hostname.assign(hostname, hostnameLength);
    }

    QueryOsVersionString(out.osVersion);
    QueryCpuModel(out.cpuModel);

    out.logicalCores = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (out.logicalCores == 0) {
        SYSTEM_INFO si{};
        ::GetSystemInfo(&si);
        out.logicalCores = si.dwNumberOfProcessors;
    }
    out.physicalCores = CountPhysicalCores();

    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (::GlobalMemoryStatusEx(&memory)) {
        out.totalMemoryBytes = memory.ullTotalPhys;
    }

    out.uptimeSeconds = ::GetTickCount64() / 1000ULL;

    if (out.hostname.empty()) {
        SetError(errorOut,
                 L"GetComputerNameW failed: " +
                     FormatWin32Error(::GetLastError()));
        return false;
    }
    return true;
}

bool QueryMemoryStatus(MemoryStatus& out, std::wstring* errorOut) {
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (!::GlobalMemoryStatusEx(&memory)) {
        SetError(errorOut,
                 L"GlobalMemoryStatusEx failed: " +
                     FormatWin32Error(::GetLastError()));
        return false;
    }

    out.totalBytes     = memory.ullTotalPhys;
    out.availableBytes = memory.ullAvailPhys;
    out.usedBytes      = out.totalBytes - out.availableBytes;
    out.usagePercent   = Percentage(out.usedBytes, out.totalBytes);
    return true;
}

bool QueryDisks(std::vector<DiskStatus>& out, std::wstring* errorOut) {
    const DWORD mask = ::GetLogicalDrives();
    if (mask == 0) {
        SetError(errorOut,
                 L"GetLogicalDrives failed: " +
                     FormatWin32Error(::GetLastError()));
        return false;
    }

    for (wchar_t letter = L'A'; letter <= L'Z'; ++letter) {
        if ((mask & (1u << (letter - L'A'))) == 0) {
            continue;
        }

        std::wstring root;
        root += letter;
        root += L":\\";

        const UINT driveType = ::GetDriveTypeW(root.c_str());
        if (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE) {
            continue;
        }

        ULARGE_INTEGER freeToCaller{};
        ULARGE_INTEGER total{};
        ULARGE_INTEGER totalFree{};
        if (!::GetDiskFreeSpaceExW(root.c_str(), &freeToCaller, &total,
                                   &totalFree)) {
            continue;  // e.g. an empty card reader; skip it silently
        }
        if (total.QuadPart == 0) {
            continue;
        }

        DiskStatus disk;
        disk.mountPoint   = root;
        disk.totalBytes   = total.QuadPart;
        disk.freeBytes    = freeToCaller.QuadPart;
        disk.usagePercent = Percentage(total.QuadPart - freeToCaller.QuadPart,
                                       total.QuadPart);
        out.push_back(std::move(disk));
    }

    if (out.empty()) {
        SetError(errorOut, L"no ready fixed or removable volume was found");
        return false;
    }
    return true;
}

std::optional<double> CpuSampler::Sample() {
    FILETIME idleTime{};
    FILETIME kernelTime{};
    FILETIME userTime{};
    if (!::GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return std::nullopt;
    }

    const std::uint64_t idle   = FileTimeToTicks(idleTime);
    const std::uint64_t kernel = FileTimeToTicks(kernelTime);
    const std::uint64_t user   = FileTimeToTicks(userTime);

    if (!hasBaseline_) {
        prevIdle_    = idle;
        prevKernel_  = kernel;
        prevUser_    = user;
        hasBaseline_ = true;
        return std::nullopt;
    }

    const std::uint64_t idleDelta   = idle - prevIdle_;
    const std::uint64_t kernelDelta = kernel - prevKernel_;
    const std::uint64_t userDelta   = user - prevUser_;

    prevIdle_   = idle;
    prevKernel_ = kernel;
    prevUser_   = user;

    // On Windows the kernel time *includes* idle time, so total CPU time is
    // kernel + user and busy time is total - idle.
    const std::uint64_t total = kernelDelta + userDelta;
    if (total == 0) {
        return std::nullopt;  // no measurable CPU time in this window
    }

    const std::uint64_t busy = (total > idleDelta) ? total - idleDelta : 0;
    double percent = static_cast<double>(busy) / static_cast<double>(total) *
                     100.0;
    percent = std::clamp(percent, 0.0, 100.0);
    return percent;
}

void CpuSampler::Reset() noexcept {
    hasBaseline_ = false;
    prevIdle_    = 0;
    prevKernel_  = 0;
    prevUser_    = 0;
}

}  // namespace sysmon
