/*
 * test_display.c — Nextion display test harness, SPRO2
 * ======================================================
 *
 * PURPOSE
 *   Drop-in replacement for main.c that walks every Nextion page in order,
 *   writes known fake values to all number components, then waits for each
 *   expected button press and reports PASS / WRONG over serial.
 *
 * HOW TO USE
 *   1. Back up src/main.c  →  src/main.c.bak
 *   2. Copy this file to src/main.c  (or rename test_display.c → main.c)
 *   3. Connect display and flash with PlatformIO
 *   4. Open a serial monitor at 9600 baud (same line as Nextion TX)
 *   5. Follow instructions printed to serial; press each prompted button
 *   6. When all pages show PASS, the driver + HMI agree
 *
 * TO SKIP A PAGE
 *   Comment out the matching test_pageN() call in main() at the bottom.
 *
 * SERIAL OUTPUT FORMAT
 *   === PAGE N: <name> ===
 *     Check  : <visual description of what to see on screen>
 *     Waiting: ACTION_XYZ
 *     GOT    : ACTION_XYZ
 *     PASS
 *
 * FAKE DATA FORMULAE
 *   Page 7 Results1 — row N (N = 1 … 10):
 *     RPM            = N * 100     (100 … 1000)
 *     Torque  [mNm]  = N * 10      (10  …  100)
 *     Efficiency [%] = N * 5       (5   …   50)
 *     Max Voltage[mV]= N * 1000    (1000…10000)
 *     Max Current[mA]= N * 100     (100 … 1000)
 *
 *   Page 8 Results2 — row N:
 *     Power     [mW] = N * 200     (200 … 2000)
 *     Mech Power[mW] = N * 150     (150 … 1500)
 *
 * DEPENDENCIES
 *   nextion.h / nextion.c / uart.c — no motor.h, opto.h, or ina219.h needed
 *
 * TARGET
 *   ATmega328 (Arduino Nano), 16 MHz, PlatformIO / avr-gcc
 */

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "nextion.h"

// ── Print the symbolic name of any ACTION_* code ──────────────────────────────
static void print_action_name(uint8_t action)
{
    switch (action) {
        case ACTION_DONE:          printf("ACTION_DONE\r\n");          break;
        case ACTION_WEIGHT_MINUS:  printf("ACTION_WEIGHT_MINUS\r\n");  break;
        case ACTION_WEIGHT_PLUS:   printf("ACTION_WEIGHT_PLUS\r\n");   break;
        case ACTION_WEIGHT_NEXT:   printf("ACTION_WEIGHT_NEXT\r\n");   break;
        case ACTION_RES_PLUS10:    printf("ACTION_RES_PLUS10\r\n");    break;
        case ACTION_RES_PLUS1:     printf("ACTION_RES_PLUS1\r\n");     break;
        case ACTION_RES_MINUS10:   printf("ACTION_RES_MINUS10\r\n");   break;
        case ACTION_RES_MINUS1:    printf("ACTION_RES_MINUS1\r\n");    break;
        case ACTION_RES_NEXT:      printf("ACTION_RES_NEXT\r\n");      break;
        case ACTION_SAFETY_YES:    printf("ACTION_SAFETY_YES\r\n");    break;
        case ACTION_START:         printf("ACTION_START\r\n");         break;
        case ACTION_TEST_NEXT:     printf("ACTION_TEST_NEXT\r\n");     break;
        case ACTION_RESULTS1_NEXT: printf("ACTION_RESULTS1_NEXT\r\n"); break;
        case ACTION_RESULTS2_BACK: printf("ACTION_RESULTS2_BACK\r\n"); break;
        case ACTION_RESULTS2_NEXT: printf("ACTION_RESULTS2_NEXT\r\n"); break;
        case ACTION_END_BACK:      printf("ACTION_END_BACK\r\n");      break;
        case ACTION_END_RESTART:   printf("ACTION_END_RESTART\r\n");   break;
        case ACTION_END_SAVE:      printf("ACTION_END_SAVE\r\n");      break;
        default:                   printf("UNKNOWN(0x%02X)\r\n", action); break;
    }
}

// ── Block until the expected button is pressed; print GOT / PASS / WRONG ──────
static void wait_for(uint8_t expected, const char *expected_name)
{
    printf("  Waiting: %s\r\n", expected_name);
    for (;;) {
        uint8_t got = nextion_read_action();
        if (got == ACTION_NONE) { _delay_ms(10); continue; }
        printf("  GOT    : ");
        print_action_name(got);
        if (got == expected) {
            printf("  PASS\r\n\r\n");
            return;
        }
        printf("  WRONG  -- keep trying\r\n");
    }
}

// ── Build a 10-row MeasurementLog using the standard fake-data formulae ───────
// Row index n (0-based) uses multiplier N = n+1 (1-based).
static void build_test_log(MeasurementLog *log)
{
    log->count = 10;
    uint8_t n;
    for (n = 0; n < 10; n++) {
        uint8_t N = n + 1;
        log->rows[n].rpm            = (uint16_t)N * 100u;
        log->rows[n].torque_mNm     = (uint16_t)N * 10u;
        log->rows[n].efficiency     = (uint16_t)N * 5u;
        log->rows[n].max_voltage_mV = (uint16_t)N * 1000u;
        log->rows[n].max_current_mA = (uint16_t)N * 100u;
        log->rows[n].power_mW       = (uint16_t)N * 200u;
        log->rows[n].mech_power_mW  = (uint16_t)N * 150u;
    }
}

// ── Page 0 — start ────────────────────────────────────────────────────────────
// Check: "Connect Motor" (or similar) title visible, Done button present.
// Press Done to pass.
static void test_page0(void)
{
    printf("=== PAGE 0: start ===\r\n");
    nextion_set_page(PAGE_START);
    _delay_ms(500);
    printf("  Check  : start screen visible\r\n");
    printf("  Check  : Done button present\r\n");
    wait_for(ACTION_DONE, "ACTION_DONE");
}

// ── Page 1 — error ────────────────────────────────────────────────────────────
// No buttons on this page — auto-advances after 3 seconds.
static void test_page1(void)
{
    printf("=== PAGE 1: error ===\r\n");
    nextion_set_page(PAGE_ERROR);
    _delay_ms(500);
    printf("  Check  : error message visible (no buttons)\r\n");
    printf("  Auto-advancing in 3 s ...\r\n");
    _delay_ms(3000);
    printf("  PASS (auto)\r\n\r\n");
}

// ── Page 2 — weights_in ───────────────────────────────────────────────────────
// Fake weight count = 7.  Tests minus, plus, Next in order.
static void test_page2(void)
{
    printf("=== PAGE 2: weights_in ===\r\n");
    nextion_set_page(PAGE_WEIGHTS_IN);
    nextion_update_weight_count(7);
    _delay_ms(500);
    printf("  Check  : weight count shows 7\r\n");
    printf("  Press buttons in this order: minus, plus, Next\r\n");
    wait_for(ACTION_WEIGHT_MINUS, "ACTION_WEIGHT_MINUS (b2 id=4)");
    wait_for(ACTION_WEIGHT_PLUS,  "ACTION_WEIGHT_PLUS  (b3 id=5)");
    wait_for(ACTION_WEIGHT_NEXT,  "ACTION_WEIGHT_NEXT  (b1 id=1)");
}

// ── Page 3 — resistance ───────────────────────────────────────────────────────
// Fake resistance = 42.  Tests all five buttons in order.
static void test_page3(void)
{
    printf("=== PAGE 3: resistance ===\r\n");
    nextion_set_page(PAGE_RESISTANCE);
    nextion_update_resistance(42);
    _delay_ms(500);
    printf("  Check  : resistance value shows 42\r\n");
    printf("  Press buttons in this order: +10, +1, -10, -1, Next\r\n");
    wait_for(ACTION_RES_PLUS10,  "ACTION_RES_PLUS10  (b0 id=3)");
    wait_for(ACTION_RES_PLUS1,   "ACTION_RES_PLUS1   (b3 id=6)");
    wait_for(ACTION_RES_MINUS10, "ACTION_RES_MINUS10 (b2 id=5)");
    wait_for(ACTION_RES_MINUS1,  "ACTION_RES_MINUS1  (b4 id=7)");
    wait_for(ACTION_RES_NEXT,    "ACTION_RES_NEXT    (b1 id=4)");
}

// ── Page 4 — safety ───────────────────────────────────────────────────────────
// Press Yes to pass.
static void test_page4(void)
{
    printf("=== PAGE 4: safety ===\r\n");
    nextion_set_page(PAGE_SAFETY);
    _delay_ms(500);
    printf("  Check  : safety prompt visible\r\n");
    wait_for(ACTION_SAFETY_YES, "ACTION_SAFETY_YES (b0 id=1)");
}

// ── Page 5 — Start_test ───────────────────────────────────────────────────────
// Press Start to pass.
static void test_page5(void)
{
    printf("=== PAGE 5: Start_test ===\r\n");
    nextion_set_page(PAGE_START_TEST);
    _delay_ms(500);
    printf("  Check  : start-test screen visible\r\n");
    wait_for(ACTION_START, "ACTION_START (b0 id=2)");
}

// ── Page 6 — Test ─────────────────────────────────────────────────────────────
// Counts runtimer 0 → 5 with 1 s delay between each step, then waits for Next.
static void test_page6(void)
{
    printf("=== PAGE 6: Test ===\r\n");
    nextion_set_page(PAGE_TEST);
    _delay_ms(500);
    printf("  Check  : test screen visible\r\n");
    printf("  Counting runtimer 0 -> 5 (1 s per step)...\r\n");

    uint16_t s;
    for (s = 0; s <= 5; s++) {
        nextion_update_runtime(s);
        printf("  runtimer = %u\r\n", s);
        _delay_ms(1000);
    }

    printf("  Check  : runtimer shows 5\r\n");
    wait_for(ACTION_TEST_NEXT, "ACTION_TEST_NEXT (b0 id=3)");
}

// ── Page 7 — Results1 ─────────────────────────────────────────────────────────
// Writes a 10-row grid then waits for Next.
//   Row N (1-based): RPM=N*100  Torque=N*10  Eff=N*5  MaxV=N*1000  MaxA=N*100
static void test_page7(void)
{
    printf("=== PAGE 7: Results1 (10x5 grid) ===\r\n");

    MeasurementLog log;
    build_test_log(&log);

    nextion_set_page(PAGE_RESULTS1);
    nextion_update_results1(&log);
    _delay_ms(500);

    printf("  Check  : 10 rows x 5 columns filled\r\n");
    printf("  Formula: row N (1-10): RPM=N*100  Torque=N*10  Eff=N*5%%  MaxV=N*1000  MaxA=N*100\r\n");
    printf("  Row  1 : RPM= 100  Torque=  10  Eff=  5%%  MaxV= 1000  MaxA=  100\r\n");
    printf("  Row  5 : RPM= 500  Torque=  50  Eff= 25%%  MaxV= 5000  MaxA=  500\r\n");
    printf("  Row 10 : RPM=1000  Torque= 100  Eff= 50%%  MaxV=10000  MaxA= 1000\r\n");
    wait_for(ACTION_RESULTS1_NEXT, "ACTION_RESULTS1_NEXT (b0 id=7)");
}

// ── Page 8 — Results2 ─────────────────────────────────────────────────────────
// Writes a 10-row grid then tests Back and Next in sequence.
//   Row N (1-based): Power=N*200  MechPower=N*150
static void test_page8(void)
{
    printf("=== PAGE 8: Results2 (10x2 grid) ===\r\n");

    MeasurementLog log;
    build_test_log(&log);

    /* Step 1 of 2: test Back button */
    nextion_set_page(PAGE_RESULTS2);
    nextion_update_results2(&log);
    _delay_ms(500);
    printf("  Check  : 10 rows x 2 columns filled\r\n");
    printf("  Formula: row N (1-10): Power=N*200  MechPower=N*150\r\n");
    printf("  Row  1 : Power=  200  MechPower=  150\r\n");
    printf("  Row 10 : Power= 2000  MechPower= 1500\r\n");
    printf("  Step 1/2:\r\n");
    wait_for(ACTION_RESULTS2_BACK, "ACTION_RESULTS2_BACK (b0 id=1)");

    /* Step 2 of 2: re-show page, test Next button */
    nextion_set_page(PAGE_RESULTS2);
    nextion_update_results2(&log);
    _delay_ms(500);
    printf("  Step 2/2:\r\n");
    wait_for(ACTION_RESULTS2_NEXT, "ACTION_RESULTS2_NEXT (b1 id=3)");
}

// ── Page 9 — end ──────────────────────────────────────────────────────────────
// Sets runtimer = 99, then tests Back, Save, Restart in that order.
// Page is re-shown before each button test to restore the known state.
static void test_page9(void)
{
    printf("=== PAGE 9: end ===\r\n");
    printf("  Three buttons will be tested in order: Back, Save, Restart\r\n\r\n");

    /* Step 1 of 3: Back */
    nextion_set_page(PAGE_END);
    nextion_update_end(99);
    _delay_ms(500);
    printf("  Check  : runtimer shows 99\r\n");
    printf("  Step 1/3:\r\n");
    wait_for(ACTION_END_BACK, "ACTION_END_BACK (b0 id=5)");

    /* Step 2 of 3: Save */
    nextion_set_page(PAGE_END);
    nextion_update_end(99);
    _delay_ms(500);
    printf("  Step 2/3:\r\n");
    wait_for(ACTION_END_SAVE, "ACTION_END_SAVE (b2 id=3)");

    /* Step 3 of 3: Restart */
    nextion_set_page(PAGE_END);
    nextion_update_end(99);
    _delay_ms(500);
    printf("  Step 3/3:\r\n");
    wait_for(ACTION_END_RESTART, "ACTION_END_RESTART (b1 id=4)");
}

// ── main ──────────────────────────────────────────────────────────────────────
int main(void)
{
    nextion_init();
    _delay_ms(1000);   /* let display boot */

    printf("\r\n");
    printf("====================================\r\n");
    printf("  NEXTION TEST  spro2\r\n");
    printf("  9600 baud -- all 10 pages\r\n");
    printf("====================================\r\n\r\n");

    /* Comment out any line below to skip that page */
    test_page0();
    test_page1();
    test_page2();
    test_page3();
    test_page4();
    test_page5();
    test_page6();
    test_page7();
    test_page8();
    test_page9();

    printf("====================================\r\n");
    printf("  ALL TESTS COMPLETE\r\n");
    printf("  Every PASS = driver matches HMI\r\n");
    printf("====================================\r\n");

    while (1) {}   /* halt */
    return 0;
}
