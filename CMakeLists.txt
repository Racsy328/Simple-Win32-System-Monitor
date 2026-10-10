cmake_minimum_required(VERSION 3.16)

project(SystemMonitor
    VERSION 2.0.0
    DESCRIPTION "Lightweight Win32 system monitor"
    LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "SystemMonitor targets Windows only (Win32 API).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Keep Release binaries self-contained and predictable.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Core library - platform queries, no third-party dependencies
# ---------------------------------------------------------------------------
add_library(monitor_core STATIC src/Monitor.cpp)
target_include_directories(monitor_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(monitor_core PUBLIC UNICODE _UNICODE)
target_link_libraries(monitor_core PUBLIC advapi32)

# ---------------------------------------------------------------------------
# Command line front end
# ---------------------------------------------------------------------------
add_executable(sysmon src/main.cpp)
target_link_libraries(sysmon PRIVATE monitor_core)

# ---------------------------------------------------------------------------
# Unit tests (opt out with -DSYSMON_BUILD_TESTS=OFF)
# ---------------------------------------------------------------------------
option(SYSMON_BUILD_TESTS "Build the unit tests" ON)
if(SYSMON_BUILD_TESTS)
    enable_testing()
    add_executable(sysmon_tests tests/test_monitor.cpp)
    target_link_libraries(sysmon_tests PRIVATE monitor_core)
    add_test(NAME monitor_unit COMMAND sysmon_tests)
endif()

# ---------------------------------------------------------------------------
# MSVC: UTF-8 sources (the tree is UTF-8 with a BOM, this makes it explicit),
# a strict warning level and standards conformance.
# ---------------------------------------------------------------------------
if(MSVC)
    set(SYSMON_MSVC_FLAGS /utf-8 /W4 /permissive- /Zc:__cplusplus)

    foreach(target monitor_core sysmon sysmon_tests)
        if(TARGET ${target})
            target_compile_options(${target} PRIVATE ${SYSMON_MSVC_FLAGS})
        endif()
    endforeach()

    foreach(target sysmon sysmon_tests)
        if(TARGET ${target})
            target_link_options(${target} PRIVATE /INCREMENTAL:NO)
        endif()
    endforeach()
else()
    # MinGW / clang builds get the same intent spelled differently.
    foreach(target monitor_core sysmon sysmon_tests)
        if(TARGET ${target})
            target_compile_options(${target} PRIVATE -Wall -Wextra)
        endif()
    endforeach()
endif()
