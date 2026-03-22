/*
 * Tick timing accuracy test.
 *
 * For each sample in xTestDelays the task:
 *   1. Records the host wall-clock time (us resolution).
 *   2. Calls vTaskDelay() for that many ticks.
 *   3. Records the wall-clock time again.
 *   4. Compares actual elapsed time against the nominal tick period.
 *
 * A result is PASS when the error is within TICK_TEST_TOLERANCE_PCT.
 * These are software simulators, so the tolerance is deliberately wide.
 *
 * Platform timing backends:
 *   POSIX   – clock_gettime(CLOCK_MONOTONIC)
 *   Windows – QueryPerformanceCounter / QueryPerformanceFrequency
 */

#include <stdint.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"

/* Local */
#include "console.h"
#include "tick_timing_test.h"

#if defined( _WIN32 )
    #include <windows.h>
#else
    #include <time.h>
#endif

/*-----------------------------------------------------------*/

/* Accepted timing error for a simulator (not a real-time target). */
#define TICK_TEST_TOLERANCE_PCT      50

#define TICK_TEST_TASK_STACK_SIZE    ( configMINIMAL_STACK_SIZE * 2 )
#define TICK_TEST_TASK_PRIORITY      ( tskIDLE_PRIORITY + 1 )

/* Tick counts to test.  Expected wall time = ticks * (1e6 / configTICK_RATE_HZ) us. */
static const TickType_t xTestDelays[] = { 10, 100, 500 };

/*-----------------------------------------------------------*/

/*
 * Returns the current host wall-clock time in microseconds.
 * The absolute value is unimportant; only differences are used.
 */
static int64_t prvGetWallTimeUs( void )
{
#if defined( _WIN32 )

    static LARGE_INTEGER xFreq = { { 0, 0 } };
    LARGE_INTEGER xNow;

    if( xFreq.QuadPart == 0 )
    {
        QueryPerformanceFrequency( &xFreq );
    }

    QueryPerformanceCounter( &xNow );
    return ( int64_t ) ( xNow.QuadPart * 1000000LL / xFreq.QuadPart );

#else /* POSIX */

    struct timespec xTs;

    clock_gettime( CLOCK_MONOTONIC, &xTs );
    return ( int64_t ) xTs.tv_sec * 1000000LL + ( int64_t ) xTs.tv_nsec / 1000LL;

#endif
}

/*-----------------------------------------------------------*/

static void prvTickTimingTestTask( void * pvParameters )
{
    const size_t xTestCount = sizeof( xTestDelays ) / sizeof( xTestDelays[ 0 ] );
    size_t i;
    int xAllPassed = 1;

    ( void ) pvParameters;

    console_print( "\r\n=== Tick Timing Test  configTICK_RATE_HZ=%u  tolerance=%d%% ===\r\n",
                   ( unsigned ) configTICK_RATE_HZ,
                   TICK_TEST_TOLERANCE_PCT );

    for( i = 0; i < xTestCount; i++ )
    {
        TickType_t xDelay = xTestDelays[ i ];
        int64_t lExpectedUs = ( int64_t ) xDelay * 1000000LL / ( int64_t ) configTICK_RATE_HZ;
        int64_t lStart, lEnd, lActualUs, lErrorUs, lErrorPct;
        const char * pcResult;

        lStart = prvGetWallTimeUs();
        vTaskDelay( xDelay );
        lEnd = prvGetWallTimeUs();

        lActualUs = lEnd - lStart;
        lErrorUs  = lActualUs - lExpectedUs;

        if( lErrorUs < 0 )
        {
            lErrorUs = -lErrorUs;
        }

        lErrorPct = ( lExpectedUs > 0 ) ? ( lErrorUs * 100LL / lExpectedUs ) : 0LL;
        pcResult  = ( lErrorPct <= TICK_TEST_TOLERANCE_PCT ) ? "PASS" : "FAIL";

        if( lErrorPct > TICK_TEST_TOLERANCE_PCT )
        {
            xAllPassed = 0;
        }

        console_print( "  delay=%4u ticks  expected=%6lld us  actual=%6lld us  error=%3lld%%  [%s]\r\n",
                       ( unsigned ) xDelay,
                       ( long long ) lExpectedUs,
                       ( long long ) lActualUs,
                       ( long long ) lErrorPct,
                       pcResult );
    }

    console_print( "=== Tick Timing Test: %s ===\r\n\r\n",
                   xAllPassed ? "PASSED" : "FAILED" );

    vTaskDelete( NULL );
}

/*-----------------------------------------------------------*/

void vStartTickTimingTest( void )
{
    xTaskCreate( prvTickTimingTestTask,
                 "TickTest",
                 TICK_TEST_TASK_STACK_SIZE,
                 NULL,
                 TICK_TEST_TASK_PRIORITY,
                 NULL );
}
