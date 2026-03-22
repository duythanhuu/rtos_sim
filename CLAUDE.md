# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Run from the parent directory (`freertos/`):

```bash
# Configure
cmake -S rtos_sim -B build/rtos_sim

# Build
cmake --build build/rtos_sim

# Run
./build/rtos_sim/bin/rtos_sim
```

Or from within this directory (`rtos_sim/`):

```bash
cmake -S . -B build/rtos_sim
cmake --build build/rtos_sim
./build/rtos_sim/bin/rtos_sim
```

### Build options

| Option | Values | Default | Notes |
|---|---|---|---|
| `RTOS_SIM_DEMO` | `FULL`, `BLINKY` | `FULL` | BLINKY is a simple echo demo; FULL runs the comprehensive test suite |
| `RTOS_SIM_ENABLE_TRACE` | `ON`, `OFF` | `OFF` | Disabled by default — vendored trace library needs alignment work |
| `RTOS_SIM_COVERAGE_TEST` | `ON`, `OFF` | `OFF` | Changes tick counter init and disables malloc-fail hook for coverage runs |
| `RTOS_SIM_TRACE_ON_ENTER` | `ON`, `OFF` | `OFF` | POSIX only: dumps trace to `Trace.dump` when Enter is pressed |

Example with options:

```bash
cmake -S rtos_sim -B build/rtos_sim -DRTOS_SIM_DEMO=BLINKY -DRTOS_SIM_COVERAGE_TEST=ON
```

## Architecture

This is a self-contained FreeRTOS simulator that runs as a native Linux (POSIX) or Windows process. It does not depend on any sources outside `rtos_sim/`.

### Directory layout

- `FreeRTOS-Kernel/` — vendored kernel. Built via its own `CMakeLists.txt` as `freertos_kernel` target. Port is selected automatically: `GCC_POSIX` on Linux (heap_3), `MSVC_MINGW` on Windows (heap_5).
- `FreeRTOS-Plus-Trace/` — vendored trace recorder library. Only compiled when `RTOS_SIM_ENABLE_TRACE=ON`.
- `source/` — simulator application entry points:
  - `main.c` — platform init, FreeRTOS hook implementations (`vApplicationIdleHook`, `vApplicationTickHook`, etc.), `vAssertCalled`
  - `main_blinky.c` — BLINKY demo implementation
  - `main_full.c` — FULL demo implementation (exercises queues, semaphores, timers, event groups, message/stream buffers, task notifications)
  - `console.c` / `console.h` — mutex-protected `console_print()` for POSIX
- `config/` — single shared `FreeRTOSConfig.h` (used by both platforms) and `Trace_Recorder_Configuration/` headers
- `platform/posix_support/` — POSIX-specific: `run-time-stats-utils.c`, `code_coverage_additions.c`
- `platform/windows_support/` — Windows-specific: `Run-time-stats-utils.c`, `code_coverage_additions.c`, modified `recmutex.c` for low tick rate
- `demo/Common/Minimal/` — shared demo task implementations used by `main_full.c`

### Key design points

`FreeRTOSConfig.h` requires `projCOVERAGE_TEST` and `projENABLE_TRACING` to be defined on the command line (it `#error`s if they are not). These are injected by `CMakeLists.txt` via `freertos_config` interface target compile definitions — do not try to define them in the header.

The demo mode (`USER_DEMO`) is passed as a compile definition with value `0` (BLINKY) or `1` (FULL) and consumed in `main.c` via `mainSELECTED_APPLICATION`.

Platform divergence is handled with `#if defined(_WIN32)` guards throughout `main.c` and `FreeRTOSConfig.h`. The POSIX port simulates each FreeRTOS task as a POSIX thread; the Windows port uses Windows threads and simulated interrupts via `vPortGenerateSimulatedInterrupt`.
