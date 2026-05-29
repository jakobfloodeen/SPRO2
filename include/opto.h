/*
 * opto.h  –  Optocoupler RPM measurement for SPRO2
 *
 * Hardware:
 *   Optocoupler output  →  D2 (PD2, INT0)
 *   One pulse per shaft revolution (adjust OPTO_PULSES_PER_REV if
 *   your encoder disc has more slots).
 *
 * Method:
 *   COUNT pulses over a 1-second window (updated by the Timer 1 ISR
 *   tick counter in main.c).
 *   RPM = pulse_count_per_second × 60 / OPTO_PULSES_PER_REV
 */

#ifndef OPTO_H
#define OPTO_H

#include <stdint.h>

/* Number of optocoupler pulses per full shaft revolution.
 * Change this to match your encoder disc slot count. */
#define OPTO_PULSES_PER_REV   1u

/**
 * opto_init()
 *   Configure INT0 (D2) as falling-edge interrupt for pulse counting.
 *   Call once in main() before enabling global interrupts.
 */
void opto_init(void);

/**
 * opto_tick_1s()
 *   Call exactly once per second (from main.c, driven by the
 *   Timer 1 ISR tick counter).
 *   Latches the current pulse count, resets the accumulator,
 *   and recalculates the RPM.
 */
void opto_tick_1s(void);

/**
 * opto_get_rpm()
 *   Returns the most recently calculated RPM value.
 *   Updated every second by opto_tick_1s().
 *   Returns 0 if no pulses were received in the last second.
 */
uint16_t opto_get_rpm(void);

#endif /* OPTO_H */
