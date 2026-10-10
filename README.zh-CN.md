# SystemMonitor

[English](README.md) | **简体中文**

[![build](https://github.com/Racsy328/Simple-Win32-System-Monitor/actions/workflows/build.yml/badge.svg)](https://github.com/Racsy328/Simple-Win32-System-Monitor/actions/workflows/build.yml)
[![license: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE.txt)
[![platform: Windows](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D6.svg)](#环境要求)

一个轻量的 Windows 系统监控工具，用 C++17 直接基于 Win32 API 编写，**不依赖任何第三方库**。
它可以报告 CPU 占用率、物理内存、磁盘容量和机器静态信息，支持实时刷新和 JSON 两种输出方式。

没有第三方依赖，不碰 WMI，不碰 COM —— 只用 SDK。

## 功能特性

- **CPU 占用率** —— 基于 `GetSystemTimes` 的差值采样，与任务管理器同源，输出 `[0, 100]` 区间内的百分比
- **内存** —— 总量 / 可用 / 已用，基于 `GlobalMemoryStatusEx`
- **磁盘** —— 所有就绪的固定盘和可移动盘，含容量、剩余空间与使用率
- **机器信息** —— 主机名、系统版本与内部版本号、CPU 型号、逻辑核数 / 物理核数、内存总量、开机时长
- **两种输出** —— 自动刷新的控制台实时视图，或每次采样输出一个 JSON 对象（便于脚本消费）
- **优雅退出** —— 响应 Ctrl+C，正常收尾

## 环境要求

- Windows 10 或 Windows 11（x64 / x86）
- 编译需要：Visual Studio 2022 并安装「使用 C++ 的桌面开发」工作负载，**或者** CMake 3.16+ 搭配任意 MSVC 工具链

## 如何构建

### Visual Studio 2022

1. 用 VS2022 打开 `SystemMonitor.sln`
2. 选择配置，例如 `Release | x64`
3. 生成解决方案

产物在 `bin\<平台>\<配置>\` 下，例如 `bin\x64\Release\SystemMonitor.exe`。

`SystemMonitorTests` 是同一个解决方案里的第二个项目，运行它即可执行单元测试。

### CMake

```bash
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

## 使用方法

### 实时视图

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

最后一行原地刷新。第一次 CPU 读数显示为 `warm`，因为占用率需要两个时间点的采样才能算出。

### JSON 输出

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

每次采样输出一行一个 JSON 对象，可以直接交给 `jq` 或任何 JSON Lines 工具处理：

```bash
SystemMonitor.exe --json --interval 500 | jq -r '.cpu_percent'
```

### 命令行参数

| 参数 | 说明 |
| --- | --- |
| `-i`, `--interval <ms>` | 刷新间隔（毫秒），默认 `1000`，范围 50–3600000 |
| `-c`, `--count <n>` | 采样 `n` 次后退出，默认一直运行到 Ctrl+C |
| `-j`, `--json` | 输出 JSON 而非实时视图 |
| `-h`, `--help` | 显示帮助 |
| `-v`, `--version` | 显示版本 |

退出码：`0` 正常，`2` 命令行参数错误。

## 项目结构

```
include/Monitor.h        对外接口：SystemInfo、MemoryStatus、DiskStatus、CpuSampler
src/Monitor.cpp          平台查询的具体实现
src/main.cpp             命令行入口（参数解析、输出渲染）
tests/test_monitor.cpp   单元测试
CMakeLists.txt           CMake 构建
SystemMonitor.sln        Visual Studio 解决方案（程序 + 测试）
```

## 设计说明

如果你打算读或扩展这份代码，有几个决定值得先了解：

- **`CpuSampler::Sample()` 返回 `std::optional<double>`。** CPU 占用率是两个时间点之间的差值，第一次调用没有可减的基准。它返回 `std::nullopt` 表示"还没有测量结果"，而不是编造一个看起来像真实读数的 `0.0`。采样窗口内没有 CPU 时间时同理。
- **失败通过返回值表达，不抛异常。** 每个查询返回 `bool` 并通过输出参数写回结果；一个可选的 `std::wstring*` 会收到由 `GetLastError` + `FormatMessageW` 生成的可读原因。
- **系统版本用 `RtlGetVersion` 获取。** `GetVersionExW` 已被弃用，且在 Windows 8.1+ 上会对没有兼容性清单的程序故意返回错误版本号。读注册表作为兜底。
- **修正了 Windows 11 的名称。** 微软一直没更新注册表里的 `ProductName`，所以 Windows 11 机器上它仍写着 `"Windows 10 ..."`。本工具用内部版本号把名称纠正过来，这也是为什么在 Win11 上会正确显示 `Windows 11 ...`。
- **源码是带 BOM 的 UTF-8，并以 `/utf-8` 编译。** 这样无论机器的 ANSI 代码页是什么，中文注释和系统里的主机名都不会乱码。

## 已知限制

- 仅支持 Windows，实现直接建立在 Win32 之上
- CPU 占用率是整机维度，不区分进程
- 暂无网络吞吐和进程级明细 —— 见下面的路线图

## 路线图

- [ ] 进程级 CPU / 内存排行（`--top`）
- [ ] 各网卡的网络吞吐
- [ ] 可配置的 JSON 字段筛选
- [ ] 支持 ANSI 终端下的彩色输出

## 参与贡献

欢迎提交 issue 和 pull request —— 详见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

[MIT](LICENSE.txt) © 2026 Racsy328
