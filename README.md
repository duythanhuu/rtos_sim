# `rtos_sim`

Standalone simulator entrypoint for this checkout.

It isolates the native FreeRTOS simulation build into one directory:

- `FreeRTOS-Kernel/` is the vendored kernel source tree used by the simulator.
- `FreeRTOS-Plus-Trace/` is the vendored trace package used when tracing is enabled.
- `platform/posix/` and `platform/windows/` hold the copied simulator application sources.
- `demo/Common/` holds the copied shared demo sources used by both simulators.
- `config/posix/` holds the Linux/POSIX simulator configuration.
- `config/windows/` holds the Windows simulator configuration.
- `CMakeLists.txt` selects the right native FreeRTOS port for the host platform and builds only from files inside `rtos_sim/`.

The project is now self-contained for simulator work. It no longer depends on source files under `../FreeRTOS/` to configure or build.
Tracing is disabled by default because the vendored `FreeRTOS-Plus-Trace` package in this checkout needs extra alignment work before the simulator demos build cleanly with tracing enabled.

## Build

Linux:

```bash
cmake -S rtos_sim -B build/rtos_sim
cmake --build build/rtos_sim
./build/rtos_sim/bin/rtos_sim
```

Windows with MinGW:

```bash
cmake -S rtos_sim -B build/rtos_sim -G "MinGW Makefiles"
cmake --build build/rtos_sim
build\\rtos_sim\\bin\\rtos_sim.exe
```

## Options

```bash
-DRTOS_SIM_DEMO=FULL|BLINKY
-DRTOS_SIM_ENABLE_TRACE=ON|OFF
-DRTOS_SIM_COVERAGE_TEST=ON|OFF
-DRTOS_SIM_TRACE_ON_ENTER=ON|OFF
```
