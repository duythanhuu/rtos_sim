#ifndef TICK_TIMING_TEST_H
#define TICK_TIMING_TEST_H

/*
 * Creates the tick timing test task.  The task delays for several known tick
 * counts, measures the actual wall-clock time elapsed using the host OS high-
 * resolution timer, and prints a PASS/FAIL result for each sample.
 *
 * Call vStartTickTimingTest() before vTaskStartScheduler().
 */
void vStartTickTimingTest( void );

#endif /* TICK_TIMING_TEST_H */
