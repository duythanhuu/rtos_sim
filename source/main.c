/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/******************************************************************************
 * This project provides two demo applications.  A simple blinky style project,
 * a more comprehensive test and demo application.
 * The mainSELECTED_APPLICATION setting is used to select between
 * the three
 *
 * If mainSELECTED_APPLICATION = BLINKY_DEMO the simple blinky demo will be built.
 * The simply blinky demo is implemented and described in main_blinky.c.
 *
 * If mainSELECTED_APPLICATION = FULL_DEMO the more comprehensive test and demo
 * application built. This is implemented and described in main_full.c.
 *
 * This file implements the code that is not demo specific, including the
 * hardware setup and FreeRTOS hook functions.
 *
 *******************************************************************************
 * NOTE: Linux will not be running the FreeRTOS demo threads continuously, so
 * do not expect to get real time behaviour from the FreeRTOS Linux port, or
 * this demo application.  Also, the timing information in the FreeRTOS+Trace
 * logs have no meaningful units.  See the documentation page for the Linux
 * port for further information:
 * https://freertos.org/FreeRTOS-simulator-for-Linux.html
 *
 *******************************************************************************
 */

/* Standard includes. */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#if defined( _WIN32 )
    #include <windows.h>
    #include <conio.h>
    #ifdef _MSC_VER
        #include <intrin.h>
    #endif
#else
    #include <unistd.h>
    #include <signal.h>
    #include <errno.h>
    #include <sys/select.h>
#endif

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Local includes. */
#include "console.h"
#include "timers.h"

#if ( projENABLE_TRACING == 1 )
    #include <trcRecorder.h>
#endif

#define BLINKY_DEMO    0
#define FULL_DEMO      1
#define mainTRACE_FILE_NAME "Trace.dump"

#if defined( _WIN32 )
    #define mainNO_KEY_PRESS_VALUE         -1
    #define mainOUTPUT_TRACE_KEY           't'
    #define mainINTERRUPT_NUMBER_KEYBOARD  3
    #define mainREGION_1_SIZE              8201
    #define mainREGION_2_SIZE              40905
    #define mainREGION_3_SIZE              50007
#endif

#ifdef BUILD_DIR
    #define BUILD         BUILD_DIR
#else
    #define BUILD         "./"
#endif

/* Demo type is passed as an argument */
#ifdef USER_DEMO
    #define     mainSELECTED_APPLICATION    USER_DEMO
#else /* Default Setting */
    #define    mainSELECTED_APPLICATION     FULL_DEMO
#endif

/* This demo uses heap_3.c (the libc provided malloc() and free()). */

/*-----------------------------------------------------------*/

extern void main_blinky( void );
extern void main_full( void );

#if defined( _WIN32 )
    extern void vBlinkyKeyboardInterruptHandler( int xKeyPressed );
    static void prvInitialiseHeap( void );
    static void prvExerciseHeapStats( void );
    static uint32_t prvKeyboardInterruptHandler( void );
    static DWORD WINAPI prvWindowsKeyboardInputThread( void * pvParam );
#else
    static void traceOnEnter( void );
#endif

/*
 * Only the comprehensive demo uses application hook (callback) functions.  See
 * https://www.FreeRTOS.org/a00016.html for more information.
 */
void vFullDemoTickHookFunction( void );
void vFullDemoIdleFunction( void );

/*
 * Prototypes for the standard FreeRTOS application hook (callback) functions
 * implemented within this file.  See http://www.freertos.org/a00016.html .
 */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask,
                                    char * pcTaskName );
void vApplicationTickHook( void );
#if defined( _WIN32 )
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    uint32_t * pulIdleTaskStackSize );
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     uint32_t * pulTimerTaskStackSize );
#else
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE * pulIdleTaskStackSize );
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     configSTACK_DEPTH_TYPE * pulTimerTaskStackSize );
#endif

#if ( projENABLE_TRACING == 1 )

    /*
     * Writes trace data to a disk file when the trace recording is stopped.
     * This function will simply overwrite any trace files that already exist.
     */
    static void prvSaveTraceFile( void );

#endif /* if ( projENABLE_TRACING == 1 ) */

#if !defined( _WIN32 )
    /*
     * Signal handler for Ctrl_C to cause the program to exit, and generate the
     * profiling info.
     */
    static void handle_sigint( int signal );
#endif

/*-----------------------------------------------------------*/

/* When configSUPPORT_STATIC_ALLOCATION is set to 1 the application writer can
 * use a callback function to optionally provide the memory required by the idle
 * and timer tasks.  This is the stack that will be used by the timer task.  It is
 * declared here, as a global, so it can be checked by a test that is implemented
 * in a different file. */
StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

#if defined( _WIN32 )
    static HANDLE xWindowsKeyboardInputThreadHandle = NULL;
    static int xKeyPressed = mainNO_KEY_PRESS_VALUE;
#endif

/*-----------------------------------------------------------*/

int main( void )
{
    #if defined( _WIN32 )
    vPortSetInterruptHandler( mainINTERRUPT_NUMBER_KEYBOARD, prvKeyboardInterruptHandler );

    xWindowsKeyboardInputThreadHandle = CreateThread( NULL,
                                                      0,
                                                      prvWindowsKeyboardInputThread,
                                                      NULL,
                                                      0,
                                                      NULL );

    fflush( stdout );
    SetThreadAffinityMask( xWindowsKeyboardInputThreadHandle, ~0x01u );
    prvInitialiseHeap();
    #else
    /* SIGINT is not blocked by the POSIX port. */
    signal( SIGINT, handle_sigint );
    #endif

    #if ( projENABLE_TRACING == 1 )
    {
        /* Initialise the trace recorder. */
        #if defined( _WIN32 )
            configASSERT( xTraceInitialize() == TRC_SUCCESS );
            configASSERT( xTraceEnable( TRC_START ) == TRC_SUCCESS );
        #else
            vTraceEnable( TRC_START );
        #endif

        printf( "\r\nTrace started.\r\n" );

        #if defined( _WIN32 )
            printf( "The trace will be dumped to disk if a call to configASSERT() fails or the '%c' key is pressed.\r\n", mainOUTPUT_TRACE_KEY );
        #else
            printf( "The trace will be dumped to disk if a call to configASSERT() fails.\r\n" );
        #endif

        #if !defined( _WIN32 ) && ( TRACE_ON_ENTER == 1 )
            printf( "\r\nThe trace will be dumped to disk if Enter is hit.\r\n" );
        #endif /* if ( TRACE_ON_ENTER == 1 ) */
    }
    #endif /* if ( projENABLE_TRACING == 1 ) */

    console_init();
    #if ( mainSELECTED_APPLICATION == BLINKY_DEMO )
    {
        console_print( "Starting echo blinky demo\n" );
        main_blinky();
    }
    #elif ( mainSELECTED_APPLICATION == FULL_DEMO )
    {
        console_print( "Starting full demo\n" );
        main_full();
    }
    #else
    {
        #error "The selected demo is not valid"
    }
    #endif /* if ( mainSELECTED_APPLICATION ) */

    return 0;
}

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
    /* vApplicationMallocFailedHook() will only be called if
     * configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
     * function that will get called if a call to pvPortMalloc() fails.
     * pvPortMalloc() is called internally by the kernel whenever a task, queue,
     * timer or semaphore is created.  It is also called by various parts of the
     * demo application.  If heap_1.c, heap_2.c or heap_4.c is being used, then the
     * size of the    heap available to pvPortMalloc() is defined by
     * configTOTAL_HEAP_SIZE in FreeRTOSConfig.h, and the xPortGetFreeHeapSize()
     * API function can be used to query the size of free heap space that remains
     * (although it does not provide information on how the remaining heap might be
     * fragmented).  See http://www.freertos.org/a00111.html for more
     * information. */
    #if defined( _WIN32 )
        vAssertCalled( __LINE__, __FILE__ );
    #else
        vAssertCalled( __FILE__, __LINE__ );
    #endif
}

/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
     * to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
     * task.  It is essential that code added to this hook function never attempts
     * to block in any way (for example, call xQueueReceive() with a block time
     * specified, or call vTaskDelay()).  If application tasks make use of the
     * vTaskDelete() API function to delete themselves then it is also important
     * that vApplicationIdleHook() is permitted to return to its calling function,
     * because it is the responsibility of the idle task to clean up memory
     * allocated by the kernel to any task that has since deleted itself. */

    #if defined( _WIN32 )
    #else
    usleep( 15000 );
    traceOnEnter();
    #endif

    #if ( mainSELECTED_APPLICATION == FULL_DEMO )
    {
        /* Call the idle task processing used by the full demo.  The simple
         * blinky demo does not use the idle task hook. */
        vFullDemoIdleFunction();
    }
    #endif
}

/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask,
                                    char * pcTaskName )
{
    ( void ) pcTaskName;
    ( void ) pxTask;

    /* Run time stack overflow checking is performed if
     * configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
     * function is called if a stack overflow is detected.  This function is
     * provided as an example only as stack overflow checking does not function
     * when running the FreeRTOS POSIX port. */
    #if defined( _WIN32 )
        vAssertCalled( __LINE__, __FILE__ );
    #else
        vAssertCalled( __FILE__, __LINE__ );
    #endif
}

/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
    /* This function will be called by each tick interrupt if
    * configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h.  User code can be
    * added here, but the tick hook is called from an interrupt context, so
    * code must not attempt to block, and only the interrupt safe FreeRTOS API
    * functions can be used (those that end in FromISR()). */

    #if ( mainSELECTED_APPLICATION == FULL_DEMO )
    {
        vFullDemoTickHookFunction();
    }
    #endif /* mainSELECTED_APPLICATION */
}

/*-----------------------------------------------------------*/

#if !defined( _WIN32 )

static void traceOnEnter( void )
{
    #if ( TRACE_ON_ENTER == 1 )
        int xReturn;
        struct timeval tv = { 0L, 0L };
        fd_set fds;

        FD_ZERO( &fds );
        FD_SET( STDIN_FILENO, &fds );

        xReturn = select( STDIN_FILENO + 1, &fds, NULL, NULL, &tv );

        if( xReturn > 0 )
        {
            #if ( projENABLE_TRACING == 1 )
            {
                prvSaveTraceFile();
            }
            #endif

            char buffer[ 1 ];
            read( STDIN_FILENO, &buffer, 1 );
        }
    #endif
}

#endif

/*-----------------------------------------------------------*/

void vLoggingPrintf( const char * pcFormat,
                     ... )
{
    va_list arg;

    va_start( arg, pcFormat );
    vprintf( pcFormat, arg );
    va_end( arg );
}

/*-----------------------------------------------------------*/

void vApplicationDaemonTaskStartupHook( void )
{
    /* This function will be called once only, when the daemon task starts to
     * execute (sometimes called the timer task).  This is useful if the
     * application includes initialisation code that would benefit from executing
     * after the scheduler has been started. */
}

/*-----------------------------------------------------------*/

#if defined( _WIN32 )

void vAssertCalled( unsigned long ulLine,
                    const char * const pcFileName )
{
    volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;

    taskENTER_CRITICAL();
    {
        printf( "ASSERT! Line %ld, file %s, GetLastError() %ld\r\n", ulLine, pcFileName, GetLastError() );
        fflush( stdout );

        #if ( projENABLE_TRACING == 1 ) && ( projCOVERAGE_TEST != 1 )
        {
            ( void ) xTraceDisable();
            prvSaveTraceFile();
            ( void ) xTraceEnable( TRC_START );
        }
        #endif

        #ifdef _MSC_VER
            __debugbreak();
        #endif
        while( ulSetToNonZeroInDebuggerToContinue == 0 )
        {
            #ifdef _MSC_VER
                __nop();
            #else
                __asm volatile ( "NOP" );
                __asm volatile ( "NOP" );
            #endif
        }
    }
    taskEXIT_CRITICAL();
}

#else

void vAssertCalled( const char * const pcFileName,
                    unsigned long ulLine )
{
    static BaseType_t xPrinted = pdFALSE;
    volatile uint32_t ulSetToNonZeroInDebuggerToContinue = 0;

    ( void ) ulLine;
    ( void ) pcFileName;

    taskENTER_CRITICAL();
    {
        if( xPrinted == pdFALSE )
        {
            xPrinted = pdTRUE;

            #if ( projENABLE_TRACING == 1 )
            {
                prvSaveTraceFile();
            }
            #endif
        }

        while( ulSetToNonZeroInDebuggerToContinue == 0 )
        {
            __asm volatile ( "NOP" );
            __asm volatile ( "NOP" );
        }
    }
    taskEXIT_CRITICAL();
}

#endif

/*-----------------------------------------------------------*/

#if ( projENABLE_TRACING == 1 )

static void prvSaveTraceFile( void )
{
    FILE * pxOutputFile;

    #if !defined( _WIN32 )
        vTraceStop();
    #endif

    pxOutputFile = fopen( mainTRACE_FILE_NAME, "wb" );

    if( pxOutputFile != NULL )
    {
        fwrite( RecorderDataPtr, sizeof( RecorderDataType ), 1, pxOutputFile );
        fclose( pxOutputFile );
        printf( "\r\nTrace output saved to %s\r\n", mainTRACE_FILE_NAME );
    }
    else
    {
        printf( "\r\nFailed to create trace dump file\r\n" );
    }
}

#endif

/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 * used by the Idle task. */
#if defined( _WIN32 )
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    uint32_t * pulIdleTaskStackSize )
#else
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
                                    StackType_t ** ppxIdleTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE * pulIdleTaskStackSize )
#endif
{
    /* If the buffers to be provided to the Idle task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
     * state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
 * application must provide an implementation of vApplicationGetTimerTaskMemory()
 * to provide the memory that is used by the Timer service task. */
#if defined( _WIN32 )
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     uint32_t * pulTimerTaskStackSize )
#else
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer,
                                     StackType_t ** ppxTimerTaskStackBuffer,
                                     configSTACK_DEPTH_TYPE * pulTimerTaskStackSize )
#endif
{
    /* If the buffers to be provided to the Timer task are declared inside this
     * function then they must be declared static - otherwise they will be allocated on
     * the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
     * task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
     * Note that, as the array is necessarily of type StackType_t,
     * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

/*-----------------------------------------------------------*/

#if !defined( _WIN32 )

static void handle_sigint( int signal )
{
    int xReturn;

    ( void ) signal;

    xReturn = chdir( BUILD ); /* Changing dir to place gmon.out inside build. */

    if( xReturn == -1 )
    {
        printf( "chdir into %s error is %d\n", BUILD, errno );
    }

    _exit( 2 );
}

#endif

/*-----------------------------------------------------------*/

#if defined( _WIN32 )

static void prvInitialiseHeap( void )
{
    static uint8_t ucHeap[ configTOTAL_HEAP_SIZE ];
    volatile uint32_t ulAdditionalOffset = 19;
    HeapStats_t xHeapStats;
    const HeapRegion_t xHeapRegions[] =
    {
        { ucHeap + 1,                                          mainREGION_1_SIZE },
        { ucHeap + 15 + mainREGION_1_SIZE,                     mainREGION_2_SIZE },
        { ucHeap + 19 + mainREGION_1_SIZE + mainREGION_2_SIZE, mainREGION_3_SIZE },
        { NULL,                                                0                 }
    };

    configASSERT( ( ulAdditionalOffset + mainREGION_1_SIZE + mainREGION_2_SIZE + mainREGION_3_SIZE ) < configTOTAL_HEAP_SIZE );
    ( void ) ulAdditionalOffset;

    vPortGetHeapStats( &xHeapStats );
    vPortDefineHeapRegions( xHeapRegions );
    prvExerciseHeapStats();
}

static void prvExerciseHeapStats( void )
{
    HeapStats_t xHeapStats;
    size_t xInitialFreeSpace = xPortGetFreeHeapSize(), xMinimumFreeBytes;
    size_t xMetaDataOverhead, i;
    void * pvAllocatedBlock;
    const size_t xArraySize = 5, xBlockSize = 1000UL;
    void * pvAllocatedBlocks[ 5 ]; /* MSVC: array size must be a compile-time constant; 5 == xArraySize */

    vPortGetHeapStats( &xHeapStats );
    configASSERT( xHeapStats.xMinimumEverFreeBytesRemaining == xHeapStats.xAvailableHeapSpaceInBytes );
    configASSERT( xHeapStats.xMinimumEverFreeBytesRemaining == xInitialFreeSpace );
    configASSERT( xHeapStats.xNumberOfSuccessfulAllocations == 0 );
    configASSERT( xHeapStats.xNumberOfSuccessfulFrees == 0 );

    pvAllocatedBlock = pvPortMalloc( xBlockSize );
    configASSERT( pvAllocatedBlock );
    xMetaDataOverhead = ( xInitialFreeSpace - xPortGetFreeHeapSize() ) - xBlockSize;

    vPortFree( pvAllocatedBlock );
    vPortGetHeapStats( &xHeapStats );
    configASSERT( xHeapStats.xAvailableHeapSpaceInBytes == xInitialFreeSpace );
    configASSERT( xHeapStats.xNumberOfSuccessfulAllocations == 1 );
    configASSERT( xHeapStats.xNumberOfSuccessfulFrees == 1 );

    for( i = 0; i < xArraySize; i++ )
    {
        pvAllocatedBlocks[ i ] = pvPortMalloc( xBlockSize );
        configASSERT( pvAllocatedBlocks[ i ] );
        vPortGetHeapStats( &xHeapStats );
        configASSERT( xHeapStats.xMinimumEverFreeBytesRemaining == ( xInitialFreeSpace - ( ( i + 1 ) * ( xBlockSize + xMetaDataOverhead ) ) ) );
        configASSERT( xHeapStats.xMinimumEverFreeBytesRemaining == xHeapStats.xAvailableHeapSpaceInBytes );
        configASSERT( xHeapStats.xNumberOfSuccessfulAllocations == ( 2UL + i ) );
        configASSERT( xHeapStats.xNumberOfSuccessfulFrees == 1 );
    }

    configASSERT( xPortGetFreeHeapSize() == xPortGetMinimumEverFreeHeapSize() );
    xMinimumFreeBytes = xPortGetFreeHeapSize();

    for( i = 0; i < xArraySize; i++ )
    {
        vPortFree( pvAllocatedBlocks[ i ] );
        vPortGetHeapStats( &xHeapStats );
        configASSERT( xHeapStats.xAvailableHeapSpaceInBytes == ( xInitialFreeSpace - ( ( xArraySize - i - 1 ) * ( xBlockSize + xMetaDataOverhead ) ) ) );
        configASSERT( xHeapStats.xNumberOfSuccessfulAllocations == ( xArraySize + 1 ) );
        configASSERT( xHeapStats.xNumberOfSuccessfulFrees == ( 2UL + i ) );
    }

    configASSERT( xMinimumFreeBytes == xPortGetMinimumEverFreeHeapSize() );
}

static uint32_t prvKeyboardInterruptHandler( void )
{
    switch( xKeyPressed )
    {
        case mainNO_KEY_PRESS_VALUE:
            break;

        case mainOUTPUT_TRACE_KEY:
            #if ( projENABLE_TRACING == 1 ) && ( projCOVERAGE_TEST != 1 )
            portENTER_CRITICAL();
            {
                ( void ) xTraceDisable();
                prvSaveTraceFile();
                ( void ) xTraceEnable( TRC_START );
            }
            portEXIT_CRITICAL();
            #endif
            break;

        default:
            #if ( mainSELECTED_APPLICATION == BLINKY_DEMO )
                vBlinkyKeyboardInterruptHandler( xKeyPressed );
            #endif
            break;
    }

    return pdFALSE;
}

static DWORD WINAPI prvWindowsKeyboardInputThread( void * pvParam )
{
    ( void ) pvParam;

    for( ; ; )
    {
        xKeyPressed = _getch();
        vPortGenerateSimulatedInterrupt( mainINTERRUPT_NUMBER_KEYBOARD );
    }

    return ( DWORD ) -1;
}

#endif

/*-----------------------------------------------------------*/

#if ( projENABLE_TRACING == 1 )

static uint32_t ulEntryTime = 0;

void vTraceTimerReset( void )
{
    ulEntryTime = xTaskGetTickCount();
}

/*-----------------------------------------------------------*/

uint32_t uiTraceTimerGetFrequency( void )
{
    return configTICK_RATE_HZ;
}

/*-----------------------------------------------------------*/

uint32_t uiTraceTimerGetValue( void )
{
    return( xTaskGetTickCount() - ulEntryTime );
}

#endif /* if ( projENABLE_TRACING == 1 ) */

/*-----------------------------------------------------------*/
