# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.0.0] - 2026-10-10

First release with a defined public API. The monitor grew from a CPU-only
skeleton into a complete system monitor.

### Added

- Memory reporting via `GlobalMemoryStatusEx` (`QueryMemoryStatus`).
- Volume reporting for every ready fixed and removable drive (`QueryDisks`).
- Static machine information: host name, OS edition and build, CPU model,
  logical and physical core counts, total memory, uptime (`QuerySystemInfo`).
- `CpuSampler`, a stateful sampler that only reports a percentage once it has
  two samples to compare.
- Command line interface: `--interval`, `--count`, `--json`, `--help`,
  `--version`.
- JSON output mode, one object per sample.
- Graceful Ctrl+C handling.
- Dependency-free unit tests, wired into CTest.
- GitHub Actions CI building through both CMake and MSBuild.
- `CMakeLists.txt` so the project builds without Visual Studio.
- English and Simplified Chinese READMEs, `CHANGELOG.md`, `CONTRIBUTING.md`
  and `.editorconfig`.

### Changed

- **Breaking:** the CPU API moved from `SysMonitor::GetCpuUsage()` to
  `sysmon::CpuSampler::Sample()`, which returns `std::optional<double>`
  instead of returning `0.0` on failure.
- **Breaking:** the public types are now in the `sysmon` namespace.
- **Breaking:** `GetSystemInfo()` no longer returns a hard-coded string; use
  `sysmon::QuerySystemInfo()`.
- The Visual Studio project was renamed from `Github_Example Program` to
  `SystemMonitor`, and the solution now contains both the application and the
  tests.
- All sources are UTF-8 with a BOM and are compiled with `/utf-8`, so the
  project builds correctly regardless of the system code page.
- Warning level raised to `/W4` with `/permissive-`.
- Windows 11 is now reported as Windows 11 rather than Windows 10.

### Fixed

- CPU usage could divide by zero when the sample window contained no CPU time.
- The first CPU reading after construction was meaningless; it is now an
  explicit "no measurement yet".
- `GetSystemInfo()` returned a placeholder instead of real data.

### Removed

- `src/txt_simple.txt`, an empty leftover file.
- A hard-coded `F:\...` executable path and two stale file references in the
  project file, which broke the build for anyone else cloning the repository.

[Unreleased]: https://github.com/Racsy328/Simple-Win32-System-Monitor/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/Racsy328/Simple-Win32-System-Monitor/releases/tag/v2.0.0
