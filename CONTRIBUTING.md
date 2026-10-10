# Contributing

Thanks for taking the time to improve SystemMonitor. This document covers the
short version of what you need to build, test and submit a change.

## Getting set up

You need one of:

- Visual Studio 2022 with the *Desktop development with C++* workload
- CMake 3.16+ with an MSVC toolchain

Clone the repository, then pick a build path.

**Visual Studio**

Open `SystemMonitor.sln` and build. The solution contains both the application
(`SystemMonitor`) and the tests (`SystemMonitorTests`).

**CMake**

```bash
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

## Running the tests

Any change to the platform queries should come with a test in
`tests/test_monitor.cpp`. The suite is deliberately dependency-free: a `CHECK`
macro and an exit code, nothing else.

```bash
# CMake
ctest --test-dir build -C Release --output-on-failure

# or run the binary directly
./bin/x64/Release/SystemMonitorTests.exe
```

The process exits with `0` when everything passed and `1` otherwise, so it
drops straight into CI.

## Coding style

Match the surrounding code. Concretely:

- C++17, four-space indentation, 80-column soft limit.
- Everything lives in the `sysmon` namespace.
- Platform queries **return `bool` and write through an output parameter**;
  they do not throw. Attach a reason via the trailing `std::wstring* errorOut`
  when the failure is something a user could act on.
- Prefer `std::optional` over magic sentinel values such as `-1` or `0.0`.
- No third-party dependencies. That constraint is a feature of the project —
  please don't add one without discussing it in an issue first.
- Sources are UTF-8 with a BOM. The build already passes `/utf-8`; keep new
  files consistent (`.editorconfig` handles this in most editors).
- Keep the warning level clean: the project builds at `/W4` with
  `/permissive-`.

## Commit messages

Short imperative subject line, then a body explaining *why* when the change is
not obvious:

```
Add per-adapter network throughput

Sampling uses GetIfTable2 rather than the older GetIfTable because ...
```

Reference an issue number in the body when one exists.

## Pull requests

1. Branch off `main`.
2. Keep the change focused; unrelated cleanups belong in their own PR.
3. Make sure both CI jobs pass — the CMake build and the MSBuild build.
4. Update `CHANGELOG.md` under `[Unreleased]` if the change is user visible.
5. Update the READMEs if you change the command line interface or the JSON
   schema.

## Reporting bugs

Please include:

- Your Windows version and build number (`winver`),
- whether you built with MSVC or CMake,
- the exact command line you ran,
- what you expected and what happened instead.

If the problem is in the JSON output, paste the offending object — it is
already anonymised enough to be useful.
