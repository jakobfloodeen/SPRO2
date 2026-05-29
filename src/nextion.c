/**
 * nextion.c — Nextion display driver for SPRO2 Motor Test Stand
 *
 * Component IDs from Nextion Editor attribute panel:
 *
 *  Page 0  start        b1  id=2   → Done
 *  Page 1  error        (no buttons)
 *  Page 2  weights_in   n0  id=6   weight count display
 *                       b2  id=4   → minus
 *                       b3  id=5   → plus
 *                       b1  id=1   → Next
 *  Page 3  resistance   n0  id=2   resistance value display
 *                       b0  id=3   → +10
 *                       b3  id=6   → +1
 *                       b2  id=5   → -10
 *                       b4  id=7   → -1
 *                       b1  id=4   → Next
 *  Page 4  safety       b0  id=1   → yes
 *  Page 5  Start_test   b0  id=2   → Start
 *  Page 6  Test         runtimer id=2
 *                       b0  id=3   → Next
 *  Page 7  Results1     b0  id=7   → Next
 *                       RPM    column: n10(id=18)..n19(id=27)
 *                       Torque column: n20(id=28)..n29(id=37)
 *                       Ef%    column: n30(id=38)..n39(id=47)
 *                       MaxV   column: n40(id=48)..n49(id=57)
 *                       MaxA   column: n50(id=58)..n59(id=67)
 *  Page 8  Results2     b0  id=1   → Back
 *                       b1  id=3   → Next
 *                       Power     column: n10(id=16)..n19(id=25)
 *                       MechPower column: n20(id=26)..n29(id=35)
 *  Page 9  end          runtimer id=2
 *                       b0  id=5   → Back
 *                       b1  id=4   → Restart
 *                       b2  id=3   → Save
 */

#include "nextion.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <stdio.h>
#include <uart.h>
#include <util/delay.h>

// ── UART ring buffer ──────────────────────────────────────────────────────────
#define RX_BUFFER_SIZE 64
static volatile uint8_t rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;

// Every Nextion command must end with three 0xFF bytes
#define CMD(...) printf(__VA_ARGS__); printf("%c%c%c", 0xFF, 0xFF, 0xFF)

// ── ISR: store every incoming byte into the ring buffer ───────────────────────
ISR(USART_RX_vect) {
    uint8_t byte = UDR0;
    uint8_t next = (rx_head + 1) % RX_BUFFER_SIZE;
    if (next != rx_tail) {
        rx_buffer[rx_head] = byte;
        rx_head = next;
    }
}

// ── Read one byte from the ring buffer (0 = empty, 1 = byte written to *byte) ─
int nextion_uart_read_byte(uint8_t *byte) {
    if (rx_tail == rx_head) return 0;
    *byte = rx_buffer[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUFFER_SIZE;
    return 1;
}

// ── Consume bytes until three consecutive 0xFF bytes are seen ────────────────
static void flush_to_end(void) {
    uint8_t ff = 0, byte;
    while (ff < 3)
        if (nextion_uart_read_byte(&byte))
            ff = (byte == 0xFF) ? ff + 1 : 0;
}

// ── Init: set up UART at 9600 baud, enable RX interrupt, redirect printf ──────
void nextion_init(void) {
    USART_Init(9600);                                        // baud rate + TX/RX
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);  // enable RX interrupt
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);                 // 8-N-1
    UART_EnablePrintf();                                     // printf → UART
    sei();
    _delay_ms(500);
    nextion_clear_buffer();
    nextion_set_page(PAGE_START);
}

// ── Flush hardware FIFO and reset the software ring buffer ───────────────────
void nextion_clear_buffer(void) {
    while (UCSR0A & (1 << RXC0)) (void)UDR0;
    rx_head = rx_tail = 0;
}

// ── Navigate to a page; 50 ms pause lets the display finish loading ───────────
void nextion_set_page(uint8_t page) {
    CMD("page %d", page);
    _delay_ms(50);
}

// ── Parse the next touch-press packet from the ring buffer ───────────────────
// Nextion packet format: [0x65][page_id][component_id][0x01=press][FF FF FF]
// Both page_id AND component_id are used — IDs are not unique across pages.
uint8_t nextion_read_action(void) {
    uint8_t byte;
    if (!nextion_uart_read_byte(&byte)) return ACTION_NONE;

    if (byte != 0x65) { flush_to_end(); return ACTION_NONE; }

    uint8_t pkt[6];
    uint8_t got = 0;
    uint16_t timeout = 20000;
    while (got < 6 && timeout--)
        if (nextion_uart_read_byte(&pkt[got])) got++;
    if (got < 6) return ACTION_NONE;

    uint8_t page_id      = pkt[0];
    uint8_t component_id = pkt[1];
    uint8_t event        = pkt[2];

    if (event != 0x01) return ACTION_NONE;  // ignore release events

    switch (page_id) {
        case PAGE_START:                        // page 0
            if (component_id == 2) return ACTION_DONE;
            break;
        case PAGE_WEIGHTS_IN:                   // page 2
            if (component_id == 4) return ACTION_WEIGHT_MINUS;
            if (component_id == 5) return ACTION_WEIGHT_PLUS;
            if (component_id == 1) return ACTION_WEIGHT_NEXT;
            break;
        case PAGE_RESISTANCE:                   // page 3
            if (component_id == 3) return ACTION_RES_PLUS10;
            if (component_id == 6) return ACTION_RES_PLUS1;
            if (component_id == 5) return ACTION_RES_MINUS10;
            if (component_id == 7) return ACTION_RES_MINUS1;
            if (component_id == 4) return ACTION_RES_NEXT;
            break;
        case PAGE_SAFETY:                       // page 4
            if (component_id == 1) return ACTION_SAFETY_YES;
            break;
        case PAGE_START_TEST:                   // page 5
            if (component_id == 2) return ACTION_START;
            break;
        case PAGE_TEST:                         // page 6
            if (component_id == 3) return ACTION_TEST_NEXT;
            break;
        case PAGE_RESULTS1:                     // page 7
            if (component_id == 7) return ACTION_RESULTS1_NEXT;
            break;
        case PAGE_RESULTS2:                     // page 8
            if (component_id == 1) return ACTION_RESULTS2_BACK;
            if (component_id == 3) return ACTION_RESULTS2_NEXT;
            break;
        case PAGE_END:                          // page 9
            if (component_id == 5) return ACTION_END_BACK;
            if (component_id == 4) return ACTION_END_RESTART;
            if (component_id == 3) return ACTION_END_SAVE;
            break;
        default: break;
    }
    return ACTION_NONE;
}

// ── Update weight-count number on page 2 (n0, id=6) ──────────────────────────
void nextion_update_weight_count(uint8_t count) {
    CMD("n0.val=%d", count);
}

// ── Update resistance value on page 3 (n0, id=2) ─────────────────────────────
void nextion_update_resistance(uint16_t value) {
    CMD("n0.val=%d", value);
}

// ── Update elapsed-time counter on page 6 (runtimer, id=2) ───────────────────
void nextion_update_runtime(uint16_t seconds) {
    CMD("runtimer.val=%d", seconds);
}

// ── Append one row to the log; silently ignores the call when log is full ─────
void nextion_log_add_row(MeasurementLog *log, const MeasurementRow *row) {
    if (log->count >= 10) return;
    log->rows[log->count] = *row;
    log->count++;
}

// ── Write all log rows to Results1 (page 7) number grid ──────────────────────
//
//  Row i uses components:
//    n1i (RPM)   n2i (Torque)   n3i (Ef%)   n4i (MaxV)   n5i (MaxA)
//
//  Examples: row 0 → n10 n20 n30 n40 n50
//            row 5 → n15 n25 n35 n45 n55
//            row 9 → n19 n29 n39 n49 n59
void nextion_update_results1(const MeasurementLog *log) {
    uint8_t i;
    for (i = 0; i < log->count && i < 10; i++) {
        CMD("n1%d.val=%u", i, log->rows[i].rpm);
        CMD("n2%d.val=%u", i, log->rows[i].torque_mNm);
        CMD("n3%d.val=%u", i, log->rows[i].efficiency);
        CMD("n4%d.val=%u", i, log->rows[i].max_voltage_mV);
        CMD("n5%d.val=%u", i, log->rows[i].max_current_mA);
    }
}

// ── Write all log rows to Results2 (page 8) number grid ──────────────────────
//
//  Row i uses components:
//    n1i (Power)   n2i (MechPower)
//
//  Examples: row 0 → n10 n20
//            row 9 → n19 n29
void nextion_update_results2(const MeasurementLog *log) {
    uint8_t i;
    for (i = 0; i < log->count && i < 10; i++) {
        CMD("n1%d.val=%u", i, log->rows[i].power_mW);
        CMD("n2%d.val=%u", i, log->rows[i].mech_power_mW);
    }
}

// ── Update elapsed-time counter on page 9 / end (runtimer, id=2) ─────────────
void nextion_update_end(uint16_t runtime_s) {
    CMD("runtimer.val=%d", runtime_s);
}
