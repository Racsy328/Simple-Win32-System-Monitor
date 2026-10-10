# SystemMonitor

**English** | [简体中文](README.zh-CN.md)

[![build](https://github.com/Racsy328/Simple-Win32-System-Monitor/actions/workflows/build.yml/badge.svg)](https://github.com/Racsy328/Simple-Win32-System-Monitor/actions/workflows/build.yml)
[![license: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.txt)
[![platform: Windows](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6.svg)](#requirements)

A small, dependency-free system monitor for Windows, written in C++17 on top of
the plain Win32 API. It reports CPU usage, physical memory, mounted volumes and
static machine information, either as a live view or as machine-readable JSON.

No third-party libraries, no WMI, no COM — just the SDK.

## Features

- **CPU usage** — sampled with `GetSystemTimes`, the same delta-based method
  Task Manager uses. Reports a percentage in `[0, 100]`.
- **Memory** — total / available / used, via `GlobalMemoryStatusEx`.
- **Volumes** — every ready fixed or removable drive with capacity, free space
  and usage percentage.
- **Machine info** — host name, OS edition and build, CPU model, logical and
  physical core counts, total RAM, uptime.
- **Two output modes** — a live, self-refreshing console view, or one JSON
  object per sample for scripting and dashboards.
- **Clean shutdown** — Ctrl+C is handled and exits gracefully.

## Screenshot

![SystemMonitor live view](docs/PRT%20SC%20SYSRQ.png)

## Requirements

- Windows 10 or Windows 11 (x64 or x86)
- To build: Visual Studio 2022 with the *Desktop development with C++*
  workload, **or** CMake 3.16+ with any MSVC toolchain

## Build

### Visual Studio 2022

1. Open `SystemMonitor.sln`.
2. Select a configuration such as `Release | x64`.
3. Build the solution.

The binaries land in `bin\<platform>\<configuration>\`, for example
`bin\x64\Release\SystemMonitor.exe`.

`SystemMonitorTests` is a second project in the same solution; run it to
execute the unit tests.

### CMake

```bash
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## Usage

### Live view

```
SystemMonitor.exe
```

```
SystemMonitor v2.0.0 - press Ctrl+C to stop

  Host     DESKTOP-EXAMPLE
  OS       Windows 11 Pro 24H2 (Build 26100)
  CPU      12th Gen Intel(R) Core(TM) i7-12700H
  Cores    20 logical / 14 physical
  Memory   31.7 GB
  Uptime   03h 12m

  CPU  12.4%   Mem  48.1% (15.2 GB)
```

The bottom line refreshes in place. The first CPU reading is marked `warm`
because a usage figure needs two samples separated in time.

### JSON output

```bash
SystemMonitor.exe --json --count 1
```

```json
{
  "version": "2.0.0",
  "cpu_percent": 12.43,
  "memory": {
    "total_bytes": 34018017280,
    "available_bytes": 17653637120,
    "used_bytes": 16364380160,
    "usage_percent": 48.10
  },
  "disks": [
    {
      "mount_point": "C:\\",
      "total_bytes": 511000000000,
      "free_bytes": 194000000000,
      "usage_percent": 62.03
    }
  ],
  "system": {
    "hostname": "DESKTOP-EXAMPLE",
    "os": "Windows 11 Pro 24H2 (Build 26100)",
    "cpu_model": "12th Gen Intel(R) Core(TM) i7-12700H",
    "logical_cores": 20,
    "physical_cores": 14,
    "uptime_seconds": 11520
  }
}
```

One object is printed per sample, one per line, so the output is ready to be
piped into `jq` or any JSON-lines consumer:

```bash
SystemMonitor.exe --json --interval 500 | jq -r '.cpu_percent'
```

### Options

| Option | Description |
| --- | --- |
| `-i`, `--interval <ms>` | Refresh interval in milliseconds (default `1000`; range 50–3600000) |
| `-c`, `--count <n>` | Stop after `n` samples (default: run until Ctrl+C) |
| `-j`, `--json` | Emit one JSON object per sample instead of the live view |
| `-h`, `--help` | Show help |
| `-v`, `--version` | Show version |

Exit codes: `0` success, `2` invalid command line.

## Project layout

```
include/Monitor.h        Public API: SystemInfo, MemoryStatus, DiskStatus, CpuSampler
src/Monitor.cpp          Implementations of the platform queries
src/main.cpp             Command line front end (argument parsing, rendering)
tests/test_monitor.cpp   Unit tests
docs/                    Screenshots and other documentation assets
CMakeLists.txt           CMake build
SystemMonitor.sln        Visual Studio solution (app + tests)
```

## Design notes

A few decisions worth knowing about if you intend to read or extend the code:

- **`CpuSampler::Sample()` returns `std::optional<double>`.** CPU usage is a
  delta between two points in time, so the first call has nothing to subtract
  from. It returns `std::nullopt` to say "no measurement yet" rather than
  inventing a `0.0` that looks like a real reading. The same applies when the
  sample window contains no CPU time.
- **Failures are return values, not exceptions.** Every query returns `bool`
  and writes through an output parameter; an optional `std::wstring*` receives
  a human-readable reason built from `GetLastError` and `FormatMessageW`.
- **The OS version comes from `RtlGetVersion`.** `GetVersionExW` is deprecated
  and deliberately reports a wrong version on Windows 8.1+ unless the binary
  carries a compatibility manifest. A registry read is used as a fallback.
- **Windows 11 names are corrected.** Microsoft never updated the
  `ProductName` registry value, so Windows 11 machines still report
  `"Windows 10 ..."` there. The build number is used to fix this up, which is
  why the tool prints `Windows 11 ...` on a Windows 11 machine.
- **Sources are UTF-8 with a BOM and compiled with `/utf-8`.** This keeps the
  Chinese comments and the file system/host names intact regardless of the
  machine's ANSI code page.

## Limitations

- Windows only. The implementation is built directly on Win32.
- The CPU percentage is system-wide, not per-process.
- No network throughput or per-process breakdown yet — see the roadmap.

## Roadmap

- [ ] Per-process CPU and memory table (`--top`)
- [ ] Network throughput per adapter
- [ ] Configurable JSON field selection
- [ ] Colour output on terminals that support ANSI escapes

## Contributing

Bug reports and pull requests are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

[MIT](LICENSE.txt) © 2026 Racsy328
