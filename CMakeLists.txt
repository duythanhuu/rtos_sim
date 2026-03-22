cmake_minimum_required(VERSION 3.20)

project(rtos_sim C)

set(RTOS_SIM_DEMO "FULL" CACHE STRING "Simulator demo to build: FULL or BLINKY")
set_property(CACHE RTOS_SIM_DEMO PROPERTY STRINGS FULL BLINKY)

option(RTOS_SIM_ENABLE_TRACE "Enable FreeRTOS+Trace recorder integration" OFF)
option(RTOS_SIM_COVERAGE_TEST "Build with coverage-test configuration" OFF)
option(RTOS_SIM_TRACE_ON_ENTER "Dump the trace when Enter is pressed on the POSIX simulator" OFF)

set(RTOS_SIM_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(FREERTOS_KERNEL_PATH ${RTOS_SIM_ROOT}/FreeRTOS-Kernel)
set(FREERTOS_TRACE_PATH ${RTOS_SIM_ROOT}/FreeRTOS-Plus-Trace)
set(RTOS_SIM_PLATFORM_ROOT ${RTOS_SIM_ROOT}/platform)
set(RTOS_SIM_SOURCE_DIR ${RTOS_SIM_ROOT}/source)
set(RTOS_SIM_POSIX_SUPPORT_DIR ${RTOS_SIM_PLATFORM_ROOT}/posix_support)
set(RTOS_SIM_WINDOWS_SUPPORT_DIR ${RTOS_SIM_PLATFORM_ROOT}/windows_support)
set(FREERTOS_COMMON_MINIMAL_PATH ${RTOS_SIM_ROOT}/demo/Common/Minimal)
set(FREERTOS_COMMON_INCLUDE_PATH ${RTOS_SIM_ROOT}/demo/Common/include)

if(NOT EXISTS ${FREERTOS_KERNEL_PATH}/CMakeLists.txt)
    message(FATAL_ERROR "FreeRTOS kernel sources not found at ${FREERTOS_KERNEL_PATH}")
endif()

if(WIN32)
    set(RTOS_SIM_PLATFORM windows)
    set(RTOS_SIM_PORT MSVC_MINGW)
    set(RTOS_SIM_HEAP 5)
    set(RTOS_SIM_SUPPORT_DIR ${RTOS_SIM_WINDOWS_SUPPORT_DIR})
elseif(UNIX)
    set(RTOS_SIM_PLATFORM posix)
    set(RTOS_SIM_PORT GCC_POSIX)
    set(RTOS_SIM_HEAP 3)
    set(RTOS_SIM_SUPPORT_DIR ${RTOS_SIM_POSIX_SUPPORT_DIR})
else()
    message(FATAL_ERROR "Unsupported host platform for rtos_sim")
endif()

set(RTOS_SIM_CONFIG_DIR ${RTOS_SIM_ROOT}/config)

if(NOT EXISTS ${RTOS_SIM_CONFIG_DIR}/FreeRTOSConfig.h)
    message(FATAL_ERROR "Missing platform config: ${RTOS_SIM_CONFIG_DIR}/FreeRTOSConfig.h")
endif()

if(RTOS_SIM_DEMO STREQUAL "BLINKY")
    set(RTOS_SIM_USER_DEMO BLINKY_DEMO)
elseif(RTOS_SIM_DEMO STREQUAL "FULL")
    set(RTOS_SIM_USER_DEMO FULL_DEMO)
else()
    message(FATAL_ERROR "RTOS_SIM_DEMO must be FULL or BLINKY")
endif()

# x86 (32-bit) only — reject 64-bit configurations early.
# For MSVC use: cmake -A Win32 ...
# For GCC/Clang the -m32 flags below enforce 32-bit; CMAKE_SIZEOF_VOID_P
# reflects the host until compiler tests run, so we check after project().
if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR
        "This project targets x86 (32-bit) only.\n"
        "For MSVC / Ninja: open a 32-bit VS Developer Command Prompt, or pass -A Win32 to the VS generator.")
endif()

add_compile_options(
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<C_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<C_COMPILER_ID:GNU,Clang>:-Wpedantic>
    $<$<C_COMPILER_ID:GNU,Clang>:-m32>
    # MSVC: match the warning level and suppressions from the WIN32-MSVC reference project
    $<$<C_COMPILER_ID:MSVC>:/W4>
    $<$<C_COMPILER_ID:MSVC>:/wd4210>  # unreferenced label (linker-level, harmless)
    $<$<C_COMPILER_ID:MSVC>:/wd4255>  # no function prototype
    $<$<C_COMPILER_ID:MSVC>:/wd4574>  # macro defined to 0
    $<$<C_COMPILER_ID:MSVC>:/wd4668>  # macro not defined, replaced with 0
    $<$<C_COMPILER_ID:MSVC>:/wd4710>  # function not inlined
    $<$<C_COMPILER_ID:MSVC>:/wd4820>  # struct padding bytes added
)

if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0601)
    add_compile_definitions(
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        $<$<C_COMPILER_ID:MSVC>:WIN32_LEAN_AND_MEAN>
    )
endif()

add_library(freertos_config INTERFACE)
target_include_directories(
    freertos_config
    INTERFACE
        ${RTOS_SIM_CONFIG_DIR}
        ${RTOS_SIM_CONFIG_DIR}/Trace_Recorder_Configuration
        ${FREERTOS_TRACE_PATH}/config
        ${FREERTOS_TRACE_PATH}/include
        ${FREERTOS_TRACE_PATH}/kernelports/FreeRTOS/config
        ${FREERTOS_TRACE_PATH}/kernelports/FreeRTOS/include
)
target_compile_definitions(
    freertos_config
    INTERFACE
        projCOVERAGE_TEST=$<IF:$<BOOL:${RTOS_SIM_COVERAGE_TEST}>,1,0>
        projENABLE_TRACING=$<IF:$<BOOL:${RTOS_SIM_ENABLE_TRACE}>,1,0>
        TRACE_ON_ENTER=$<IF:$<BOOL:${RTOS_SIM_TRACE_ON_ENTER}>,1,0>
)

set(FREERTOS_HEAP ${RTOS_SIM_HEAP} CACHE STRING "" FORCE)
set(FREERTOS_PORT ${RTOS_SIM_PORT} CACHE STRING "" FORCE)

add_subdirectory(${FREERTOS_KERNEL_PATH} ${CMAKE_CURRENT_BINARY_DIR}/FreeRTOS-Kernel)

if(TARGET freertos_kernel AND RTOS_SIM_PLATFORM STREQUAL "posix")
    target_compile_options(
        freertos_kernel
        PRIVATE
            $<$<BOOL:${RTOS_SIM_ENABLE_TRACE}>:-Wno-pointer-to-int-cast>
    )
endif()

set(RTOS_SIM_MAIN_SOURCES
    ${RTOS_SIM_SUPPORT_DIR}/code_coverage_additions.c
    ${RTOS_SIM_SOURCE_DIR}/console.c
    ${RTOS_SIM_SOURCE_DIR}/tick_timing_test.c
    ${RTOS_SIM_SOURCE_DIR}/main.c
    ${RTOS_SIM_SOURCE_DIR}/main_blinky.c
    ${RTOS_SIM_SOURCE_DIR}/main_full.c
)

if(RTOS_SIM_PLATFORM STREQUAL "posix")
    list(APPEND RTOS_SIM_MAIN_SOURCES
        ${RTOS_SIM_SUPPORT_DIR}/run-time-stats-utils.c
    )

    set(RTOS_SIM_DEMO_SOURCES
        ${FREERTOS_COMMON_MINIMAL_PATH}/AbortDelay.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/BlockQ.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/blocktim.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/countsem.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/death.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/dynamic.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/EventGroupsDemo.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/flop.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/GenQTest.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/integer.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/IntSemTest.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/MessageBufferAMP.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/MessageBufferDemo.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/PollQ.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/QPeek.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/QueueOverwrite.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/QueueSet.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/QueueSetPolling.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/recmutex.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/semtest.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/StaticAllocation.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/StreamBufferDemo.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/StreamBufferInterrupt.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/TaskNotify.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/TimerDemo.c
    )
else()
    list(APPEND RTOS_SIM_MAIN_SOURCES
        ${RTOS_SIM_SUPPORT_DIR}/Run-time-stats-utils.c
    )

    file(GLOB RTOS_SIM_DEMO_SOURCES CONFIGURE_DEPENDS ${FREERTOS_COMMON_MINIMAL_PATH}/*.c)
    list(REMOVE_ITEM RTOS_SIM_DEMO_SOURCES
        ${FREERTOS_COMMON_MINIMAL_PATH}/comtest.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/comtest_strings.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/flash.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/flash_timer.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/IntQueue.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/recmutex.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/sp_flop.c
        ${FREERTOS_COMMON_MINIMAL_PATH}/TaskNotifyArray.c
    )

    # MessageBufferAMP.c uses portYIELD_FROM_ISR (expands to "return x") inside a
    # void function.  GCC 14 made -Wreturn-mismatch an error; suppress it for this
    # file only since the macro is defined that way by the Windows port.
    set_source_files_properties(
        ${FREERTOS_COMMON_MINIMAL_PATH}/MessageBufferAMP.c
        PROPERTIES COMPILE_OPTIONS "-Wno-return-mismatch"
    )
    list(APPEND RTOS_SIM_DEMO_SOURCES
        ${RTOS_SIM_SUPPORT_DIR}/DemosModifiedForLowTickRate/recmutex.c
    )
endif()

if(RTOS_SIM_ENABLE_TRACE)
    file(GLOB RTOS_SIM_TRACE_SOURCES CONFIGURE_DEPENDS
        ${FREERTOS_TRACE_PATH}/*.c
        ${FREERTOS_TRACE_PATH}/kernelports/FreeRTOS/*.c
    )
else()
    set(RTOS_SIM_TRACE_SOURCES)
endif()

add_executable(rtos_sim
    ${RTOS_SIM_MAIN_SOURCES}
    ${RTOS_SIM_DEMO_SOURCES}
    ${RTOS_SIM_TRACE_SOURCES}
)

target_include_directories(
    rtos_sim
    PRIVATE
        ${RTOS_SIM_CONFIG_DIR}
        ${RTOS_SIM_CONFIG_DIR}/Trace_Recorder_Configuration
        ${RTOS_SIM_SOURCE_DIR}
        ${RTOS_SIM_SUPPORT_DIR}
        ${FREERTOS_TRACE_PATH}/config
        ${FREERTOS_COMMON_INCLUDE_PATH}
        ${FREERTOS_TRACE_PATH}/include
        ${FREERTOS_TRACE_PATH}/kernelports/FreeRTOS/config
        ${FREERTOS_TRACE_PATH}/streamports/File/include
        ${FREERTOS_TRACE_PATH}/streamports/File/config
)

target_compile_definitions(
    rtos_sim
    PRIVATE
        USER_DEMO=$<IF:$<STREQUAL:${RTOS_SIM_USER_DEMO},BLINKY_DEMO>,0,1>
    )

target_link_libraries(rtos_sim PRIVATE freertos_kernel freertos_config)

# Force 32-bit output for GCC/Clang (POSIX and MinGW).
target_link_options(rtos_sim PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-m32>
)

set_target_properties(
    rtos_sim
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
