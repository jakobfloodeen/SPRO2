#ifndef NEXTION_H
#define NEXTION_H

#include <stdint.h>

// ── Page indices (0–9) ────────────────────────────────────────────────────────
//
//  #   Name          Key components
//  --  ------------  --------------------------------------------------------
//  0   start         b1(id=2) Done
//  1   error         no buttons
//  2   weights_in    n0(id=6) count  b2(id=4)−  b3(id=5)+  b1(id=1)Next
//  3   resistance    n0(id=2) value  b0(id=3)+10 b3(id=6)+1 b2(id=5)-10 b4(id=7)-1 b1(id=4)Next
//  4   safety        b0(id=1) yes
//  5   Start_test    b0(id=2) Start
//  6   Test          runtimer(id=2)  b0(id=3) Next
//  7   Results1      10×5 number grid  b0(id=7) Next
//  8   Results2      10×2 number grid  b0(id=1) Back  b1(id=3) Next
//  9   end           runtimer(id=2)    b0(id=5) Back  b1(id=4) Restart  b2(id=3) Save

#define PAGE_START       0
#define PAGE_ERROR       1
#define PAGE_WEIGHTS_IN  2
#define PAGE_RESISTANCE  3
#define PAGE_SAFETY      4
#define PAGE_START_TEST  5
#define PAGE_TEST        6
#define PAGE_RESULTS1    7
#define PAGE_RESULTS2    8
#define PAGE_END         9

// ── Action codes returned by nextion_read_action() ───────────────────────────
//
//  Code  Page  Button  id  Description
//  ----  ----  ------  --  ----------------------------
//  0x00   —     —      —   no event (idle)
//  0x01   0     b1      2   Done
//  0x02   2     b2      4   weight −
//  0x03   2     b3      5   weight +
//  0x04   2     b1      1   weight Next
//  0x05   3     b0      3   resistance +10
//  0x06   3     b3      6   resistance +1
//  0x07   3     b2      5   resistance −10
//  0x08   3     b4      7   resistance −1
//  0x09   3     b1      4   resistance Next
//  0x0A   4     b0      1   safety Yes
//  0x0B   5     b0      2   Start
//  0x0C   6     b0      3   Test Next
//  0x0D   7     b0      7   Results1 Next
//  0x0E   8     b0      1   Results2 Back
//  0x0F   8     b1      3   Results2 Next
//  0x10   9     b0      5   End Back
//  0x11   9     b1      4   End Restart
//  0x12   9     b2      3   End Save

#define ACTION_NONE             0x00

// Page 0 — start
#define ACTION_DONE             0x01   // b1  id=2

// Page 2 — weights_in
#define ACTION_WEIGHT_MINUS     0x02   // b2  id=4
#define ACTION_WEIGHT_PLUS      0x03   // b3  id=5
#define ACTION_WEIGHT_NEXT      0x04   // b1  id=1

// Page 3 — resistance
#define ACTION_RES_PLUS10       0x05   // b0  id=3
#define ACTION_RES_PLUS1        0x06   // b3  id=6
#define ACTION_RES_MINUS10      0x07   // b2  id=5
#define ACTION_RES_MINUS1       0x08   // b4  id=7
#define ACTION_RES_NEXT         0x09   // b1  id=4

// Page 4 — safety
#define ACTION_SAFETY_YES       0x0A   // b0  id=1

// Page 5 — Start_test
#define ACTION_START            0x0B   // b0  id=2

// Page 6 — Test
#define ACTION_TEST_NEXT        0x0C   // b0  id=3

// Page 7 — Results1
#define ACTION_RESULTS1_NEXT    0x0D   // b0  id=7

// Page 8 — Results2
#define ACTION_RESULTS2_BACK    0x0E   // b0  id=1
#define ACTION_RESULTS2_NEXT    0x0F   // b1  id=3

// Page 9 — end
#define ACTION_END_BACK         0x10   // b0  id=5
#define ACTION_END_RESTART      0x11   // b1  id=4
#define ACTION_END_SAVE         0x12   // b2  id=3

// ── MeasurementRow: one snapshot of all seven measured/derived channels ───────
//
//  Field           Results1 column   Results2 column
//  --------------- ----------------  ---------------
//  rpm             n10–n19 (id 18–27)
//  torque_mNm      n20–n29 (id 28–37)
//  efficiency      n30–n39 (id 38–47)  [%]
//  max_voltage_mV  n40–n49 (id 48–57)
//  max_current_mA  n50–n59 (id 58–67)
//  power_mW                           n10–n19 (id 16–25)
//  mech_power_mW                      n20–n29 (id 26–35)

typedef struct {
    uint16_t rpm;             // shaft speed                [RPM]
    uint16_t torque_mNm;      // load torque                [mNm]
    uint16_t efficiency;      // motor efficiency           [%]
    uint16_t max_voltage_mV;  // peak bus voltage           [mV]
    uint16_t max_current_mA;  // peak shunt current         [mA]
    uint16_t power_mW;        // electrical power  V×I      [mW]
    uint16_t mech_power_mW;   // mechanical power  T×ω      [mW]
} MeasurementRow;

// ── MeasurementLog: up to 10 snapshots taken during one test run ──────────────
typedef struct {
    MeasurementRow rows[10];  // rows[0] = first step, rows[count-1] = last step
    uint8_t        count;     // number of valid rows (0–10)
} MeasurementLog;

// ── Public API ────────────────────────────────────────────────────────────────
void    nextion_init(void);
void    nextion_clear_buffer(void);
void    nextion_set_page(uint8_t page);
uint8_t nextion_read_action(void);

void    nextion_update_weight_count(uint8_t count);     // page 2: n0 (id=6)
void    nextion_update_resistance(uint16_t value);      // page 3: n0 (id=2)
void    nextion_update_runtime(uint16_t seconds);       // page 6: runtimer (id=2)

void    nextion_log_add_row(MeasurementLog *log, const MeasurementRow *row);
void    nextion_update_results1(const MeasurementLog *log);  // page 7: 10×5 grid
void    nextion_update_results2(const MeasurementLog *log);  // page 8: 10×2 grid
void    nextion_update_end(uint16_t runtime_s);              // page 9: runtimer (id=2)

int     nextion_uart_read_byte(uint8_t *byte);

#endif /* NEXTION_H */
