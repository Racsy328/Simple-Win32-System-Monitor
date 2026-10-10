#include "../include/Monitor.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> g_stop{false};

BOOL WINAPI ConsoleHandler(DWORD type) {
    switch (type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            g_stop.store(true);
            return TRUE;
        default:
            return FALSE;
    }
}

// ---------------------------------------------------------------------------
// Formatting helpers
//
// The console is switched to UTF-8 and every std::wstring is converted on the
// way out, so the program avoids std::locale entirely (constructing a locale
// can throw, and it would otherwise decide how numbers are formatted).
// ---------------------------------------------------------------------------

std::string ToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int size = ::WideCharToMultiByte(
        CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0,
        nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string out(static_cast<std::size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                          static_cast<int>(text.size()), out.data(), size,
                          nullptr, nullptr);
    return out;
}

std::string FormatBytes(std::uint64_t bytes) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.1f GB",
                  static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    return buffer;
}

std::string FormatUptime(std::uint64_t seconds) {
    const std::uint64_t days    = seconds / 86400ULL;
    const std::uint64_t hours   = (seconds % 86400ULL) / 3600ULL;
    const std::uint64_t minutes = (seconds % 3600ULL) / 60ULL;

    char buffer[64] = {};
    if (days > 0) {
        std::snprintf(buffer, sizeof(buffer), "%llud %02lluh %02llum",
                      static_cast<unsigned long long>(days),
                      static_cast<unsigned long long>(hours),
                      static_cast<unsigned long long>(minutes));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%02lluh %02llum",
                      static_cast<unsigned long long>(hours),
                      static_cast<unsigned long long>(minutes));
    }
    return buffer;
}

std::string JsonEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size() + 8);
    for (const char rawChar : text) {
        const auto ch = static_cast<unsigned char>(rawChar);
        switch (ch) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (ch < 0x20) {
                    char escaped[8] = {};
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", ch);
                    out += escaped;
                } else {
                    out += rawChar;
                }
        }
    }
    return out;
}

/// Sleeps while staying responsive to Ctrl+C.
void SleepInterruptible(unsigned milliseconds) {
    constexpr unsigned kSliceMs = 50;
    for (unsigned elapsed = 0; elapsed < milliseconds && !g_stop.load();
         elapsed += kSliceMs) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kSliceMs));
    }
}

// ---------------------------------------------------------------------------
// Command line
// ---------------------------------------------------------------------------

struct Options {
    unsigned  intervalMs  = 1000;
    long long count       = -1;  // < 0 means "until interrupted"
    bool      json        = false;
    bool      showHelp    = false;
    bool      showVersion = false;
};

void PrintUsage() {
    std::printf(
        "SystemMonitor %ls - lightweight Win32 system monitor\n"
        "\n"
        "Usage: sysmon [options]\n"
        "\n"
        "Options:\n"
        "  -i, --interval <ms>   Refresh interval in milliseconds (default 1000)\n"
        "  -c, --count <n>       Stop after n samples (default: until Ctrl+C)\n"
        "  -j, --json            Emit one JSON object per sample\n"
        "  -h, --help            Show this help\n"
        "  -v, --version         Show version\n",
        sysmon::kVersion);
}

/// Returns false when the arguments are malformed.
bool ParseArgs(int argc, wchar_t** argv, Options& out) {
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];

        auto requireValue = [&](const wchar_t* name) -> const wchar_t* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "error: %ls requires a value\n", name);
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == L"-h" || arg == L"--help") {
            out.showHelp = true;
        } else if (arg == L"-v" || arg == L"--version") {
            out.showVersion = true;
        } else if (arg == L"-j" || arg == L"--json") {
            out.json = true;
        } else if (arg == L"-i" || arg == L"--interval") {
            const wchar_t* value = requireValue(L"--interval");
            if (value == nullptr) {
                return false;
            }
            wchar_t* end = nullptr;
            const long parsed = std::wcstol(value, &end, 10);
            if (end == value || *end != L'\0' || parsed < 50 ||
                parsed > 3600000) {
                std::fprintf(stderr,
                             "error: --interval must be 50..3600000 ms\n");
                return false;
            }
            out.intervalMs = static_cast<unsigned>(parsed);
        } else if (arg == L"-c" || arg == L"--count") {
            const wchar_t* value = requireValue(L"--count");
            if (value == nullptr) {
                return false;
            }
            wchar_t* end = nullptr;
            const long long parsed = std::wcstoll(value, &end, 10);
            if (end == value || *end != L'\0' || parsed < 1) {
                std::fprintf(stderr, "error: --count must be >= 1\n");
                return false;
            }
            out.count = parsed;
        } else {
            std::fprintf(stderr, "error: unknown option '%ls'\n", arg.c_str());
            std::fprintf(stderr, "try 'sysmon --help'\n");
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void PrintHumanHeader(const sysmon::SystemInfo& info) {
    std::printf("SystemMonitor v%ls - press Ctrl+C to stop\n\n",
                sysmon::kVersion);
    std::printf("  Host     %s\n", ToUtf8(info.hostname).c_str());
    std::printf("  OS       %s\n", ToUtf8(info.osVersion).c_str());
    std::printf("  CPU      %s\n", ToUtf8(info.cpuModel).c_str());
    std::printf("  Cores    %u logical / %u physical\n", info.logicalCores,
                info.physicalCores);
    std::printf("  Memory   %s\n", FormatBytes(info.totalMemoryBytes).c_str());
    std::printf("  Uptime   %s\n\n", FormatUptime(info.uptimeSeconds).c_str());
}

void PrintSampleAsJson(const std::optional<double>& cpuPercent,
                       const sysmon::MemoryStatus& memory,
                       const std::vector<sysmon::DiskStatus>& disks,
                       const sysmon::SystemInfo& info) {
    std::string line = "{\"version\":\"";
    line += ToUtf8(sysmon::kVersion);
    line += "\",\"cpu_percent\":";

    if (cpuPercent.has_value()) {
        char buffer[32] = {};
        std::snprintf(buffer, sizeof(buffer), "%.2f", *cpuPercent);
        line += buffer;
    } else {
        line += "null";  // baseline not established yet
    }

    char numbers[256] = {};
    std::snprintf(numbers, sizeof(numbers),
                  ",\"memory\":{\"total_bytes\":%llu,\"available_bytes\":%llu,"
                  "\"used_bytes\":%llu,\"usage_percent\":%.2f}",
                  static_cast<unsigned long long>(memory.totalBytes),
                  static_cast<unsigned long long>(memory.availableBytes),
                  static_cast<unsigned long long>(memory.usedBytes),
                  memory.usagePercent);
    line += numbers;

    line += ",\"disks\":[";
    for (std::size_t i = 0; i < disks.size(); ++i) {
        if (i > 0) {
            line += ",";
        }
        std::snprintf(numbers, sizeof(numbers),
                      "{\"mount_point\":\"%s\",\"total_bytes\":%llu,"
                      "\"free_bytes\":%llu,\"usage_percent\":%.2f}",
                      JsonEscape(ToUtf8(disks[i].mountPoint)).c_str(),
                      static_cast<unsigned long long>(disks[i].totalBytes),
                      static_cast<unsigned long long>(disks[i].freeBytes),
                      disks[i].usagePercent);
        line += numbers;
    }
    line += "]";

    std::snprintf(numbers, sizeof(numbers),
                  ",\"system\":{\"hostname\":\"%s\",\"os\":\"%s\","
                  "\"cpu_model\":\"%s\",\"logical_cores\":%u,"
                  "\"physical_cores\":%u,\"uptime_seconds\":%llu}}",
                  JsonEscape(ToUtf8(info.hostname)).c_str(),
                  JsonEscape(ToUtf8(info.osVersion)).c_str(),
                  JsonEscape(ToUtf8(info.cpuModel)).c_str(), info.logicalCores,
                  info.physicalCores,
                  static_cast<unsigned long long>(info.uptimeSeconds));
    line += numbers;

    std::printf("%s\n", line.c_str());
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);
    ::SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    Options options;
    if (!ParseArgs(argc, argv, options)) {
        return 2;
    }
    if (options.showHelp) {
        PrintUsage();
        return 0;
    }
    if (options.showVersion) {
        std::printf("SystemMonitor %ls\n", sysmon::kVersion);
        return 0;
    }

    sysmon::SystemInfo info;
    std::wstring error;
    if (!sysmon::QuerySystemInfo(info, &error)) {
        std::fprintf(stderr, "warning: could not read system info (%s)\n",
                     ToUtf8(error).c_str());
    }

    if (!options.json) {
        PrintHumanHeader(info);
    }

    sysmon::CpuSampler sampler;

    for (long long sample = 0;
         !g_stop.load() && (options.count < 0 || sample < options.count);
         ++sample) {
        if (sample > 0) {
            SleepInterruptible(options.intervalMs);
            if (g_stop.load()) {
                break;
            }
        }

        const std::optional<double> cpuPercent = sampler.Sample();

        sysmon::MemoryStatus memory;
        if (!sysmon::QueryMemoryStatus(memory, &error)) {
            memory = {};
        }

        std::vector<sysmon::DiskStatus> disks;
        sysmon::QueryDisks(disks, &error);

        if (options.json) {
            PrintSampleAsJson(cpuPercent, memory, disks, info);
            continue;
        }

        char line[256] = {};
        char cpuText[16] = {};
        if (cpuPercent.has_value()) {
            std::snprintf(cpuText, sizeof(cpuText), "%5.1f%%", *cpuPercent);
        } else {
            std::snprintf(cpuText, sizeof(cpuText), "  warm");
        }
        std::snprintf(line, sizeof(line), "\r  CPU %s   Mem %5.1f%% (%s)   ",
                      cpuText, memory.usagePercent,
                      FormatBytes(memory.usedBytes).c_str());
        std::fputs(line, stdout);
        std::fflush(stdout);
    }

    if (!options.json) {
        std::printf("\n\nStopped.\n");
    }
    return 0;
}
