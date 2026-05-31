#include "I2C.h"
#include "adc.h"
#include "ina219.h"
#include "motor.h"
#include "nextion.h"
#include "sd_logger.h"
#include "usart.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <math.h>
#include <stdio.h>
#include <util/delay.h>

// Definitions + Storage
#define PRESCALER 1024UL
#define TIME_CONSTANT (F_CPU / PRESCALER) // 15625 ticks/sec
#define HOLES_PER_REV 10
#define MAX_BUFFER_SIZE 20
#define AVERAGING_WINDOW 5
#define BAUDRATE 9600
#define RECORD_NUMBER 10 // keep less than 100
#define LENGTH 0.047     // Length of arm that force travels on

// Logging fields
#define F_ANGV 0
#define F_TORQUE 1
#define F_EFF 2
#define F_VOLT 3
#define F_AMP 4
#define F_EPWR 5
#define F_MPWR 6

/* OPTO */
/* OPTO */
volatile uint16_t last_time0 = 0;
volatile uint16_t last_time1 = 0;
volatile float rps0[MAX_BUFFER_SIZE];
volatile float rps1[MAX_BUFFER_SIZE];
volatile uint8_t index0 = 0;
volatile uint8_t index1 = 0;

/* MOTOR */
uint8_t PWM_duty = 100 / RECORD_NUMBER; // Starting value

/* STORAGE */
float record_voltage[RECORD_NUMBER]; // records to hold "X", for "Y" values
float record_current[RECORD_NUMBER];
float record_power_elec[RECORD_NUMBER];
float record_power_mech[RECORD_NUMBER];
float record_RPM[RECORD_NUMBER];
float record_torque[RECORD_NUMBER];
float record_force[RECORD_NUMBER];
float record_duty[RECORD_NUMBER];

volatile uint16_t isr0_count = 0;
volatile uint16_t isr1_count = 0;

/* NEXTION */
uint8_t PAGE_NUMBER = 0;
uint16_t nextion_resistance = 500; // PRE VALUE
uint8_t nextion_weight = 50;       // PRE VALUE

float average(volatile float *, uint8_t, uint8_t);

// End of Definitions + storage

// move stack to end of SRAM
// FIXME: try to remove this line, see if SD card still works
void __attribute__((naked, section(".init3"))) stack_init(void) { SP = RAMEND; }

// Main
// int main(void) {
//   sd_logger_init("a,b,c");
//   _delay_ms(1000);

int main(void) {
  uart_init();

  init_display();
  TWIInit(100000); // (100kHz)
  INA219_init();
  pwm1_init();
  adc_init();

  static const char header[] = "a_v,tor,eff,vol,amp,epw,mpw";
  sd_logger_init(header);

  float row[LOG_FIELDS];

  // uint8_t row_count = 0;
  // static char row[] = "1,2,3";

  // while (1) {
  //   PORTB |= (1 << PB5);
  //   _delay_ms(500);
  //   PORTB &= ~(1 << PB5);
  //   _delay_ms(500);
  //   PORTB |= (1 << PB5);
  //   _delay_ms(500);
  //   PORTB &= ~(1 << PB5);
  //   _delay_ms(500);
  //   PORTB |= (1 << PB5);
  //   _delay_ms(500);
  //   PORTB &= ~(1 << PB5);
  //   _delay_ms(500);
  //
  // sd_logger_write(row);
  // _delay_ms(1000);

  // External Interrupt Control Register A
  EICRA = (1 << ISC00) | (1 << ISC01) | (1 << ISC10) |
          (1 << ISC11); // Rising edge, The rising edge of INT1 generates an
                        // interrupt request.
  // External Interrupt Mask Register
  EIMSK =
      (1 << INT0) | (1 << INT1); // External Interrupt Request 0 and 1 Enabled

  sei();
  // End of Optocoupler Main

  // Start of Nextion Main
  while (1) {

    uint8_t cont = 0;

    // weight screen
    while (!cont) {
      uint8_t action = read_value();
      switch (action) {
      case 0xd:
        nextion_weight = get_value("n0");
        set_page(3);
        cont = 1;
        break;

      default:
        break;
      }
      _delay_ms(25);
    }

    // resistance screen
    cont = 0;
    do {
      uint8_t action = read_value();
      switch (action) {
      case 0xd:
        nextion_resistance = get_value("n0");
        set_page(4);
        cont = 1;
        break;

      default:
        break;
      }
    } while (!cont);

    // safety and start screen
    cont = 0;
    do {
      uint8_t action = read_value();
      switch (action) {
      case 0xa:
        set_page(6);
        cont = 1;
        break;

      default:
        break;
      }
    } while (!cont);

    // UPDATE RUN SCREEN vvvvvvv

    // increases duty cycle for PWM by 1 each second and measures RPM, Voltage,
    // Current and pressure on FSR
    int display_value = 0;
    for (int i = 0; i < RECORD_NUMBER; i++) {

      set_value("runtimer", display_value);

      pwm1_set_duty(PWM_duty * i);     // set duty cycle
      record_duty[i] = (PWM_duty * i); // %
      _delay_ms(1000);
      display_value++;
      record_voltage[i] = INA219_get_bus_voltage(); // V
      record_current[i] = INA219_get_current();     // mA
      record_power_elec[i] = INA219_get_power();    // mW
      record_RPM[i] = (average(rps0, index0, AVERAGING_WINDOW) +
                       average(rps1, index1, AVERAGING_WINDOW)) /
                      2; // RPM (average of both optos)
      record_force[i] =
          adc_to_voltage(adc_read()); // value from 0 - (2^16)-1 (max is 5V). 1
                                      // = 0.07629394531mV, (2^16)-1 = 5000mV

      // printf("%.4f\n", adc_to_voltage(adc_read()));

      row[F_ANGV] = record_RPM[i];
      row[F_TORQUE] = record_torque[i];
      row[F_EFF] = record_power_mech[i] / record_power_elec[i];
      row[F_VOLT] = record_voltage[i];
      row[F_EPWR] = record_power_elec[i];
      row[F_MPWR] = record_power_mech[i];

      sd_logger_write_row(row);
      _delay_ms(1000); // delay for writing to sd card
    }

    set_page(7);
    pwm1_set_duty(0); // stop motor

    sd_logger_close();

    // DISPLAY RESULTS

    // RECORDS RPM
    for (int i = 0; i < RECORD_NUMBER; i++) {
      char location[4];

      location[0] = 'n';
      location[1] = '1';
      location[2] = '0' + i;
      location[3] = '\0';

      set_value(location, record_RPM[i]);
      _delay_ms(25);
    }
    // RECORDS TORQUE
    for (int i = 0; i < RECORD_NUMBER; i++) {
      char location[4];

      location[0] = 'n';
      location[1] = '2';
      location[2] = '0' + i;
      location[3] = '\0';

      set_value(location, record_torque[i]);
      _delay_ms(25);
    }
    // RECORDS force
    for (int i = 0; i < RECORD_NUMBER; i++) {
      char location[4];

      location[0] = 'n';
      location[1] = '3';
      location[2] = '0' + i;
      location[3] = '\0';

      set_value(location, record_force[i]);
      _delay_ms(25);
    }
    // RECORDS MAX V
    for (int i = 0; i < RECORD_NUMBER; i++) {
      char location[4];

      location[0] = 'n';
      location[1] = '4';
      location[2] = '0' + i;
      location[3] = '\0';

      set_value(location, record_voltage[i]);
      _delay_ms(25);
    }
    // RECORDS MAX A
    for (int i = 0; i < RECORD_NUMBER; i++) {
      char location[4];

      location[0] = 'n';
      location[1] = '5';
      location[2] = '0' + i;
      location[3] = '\0';

      set_value(location, record_current[i]);
      _delay_ms(25);
    }

    // FROM PAGE 7 to PAGE 8
    cont = 0;
    do {
      uint8_t action = read_value();
      switch (action) {
      case 0xd:
        set_page(8);
        cont = 1;
        break;

      default:
        break;
      }
    } while (!cont);

    // RECORDS ELEC-POWER
    for (int i = 0; i < RECORD_NUMBER; i++) {
      char location[4];

      location[0] = 'n';
      location[1] = '1';
      location[2] = '0' + i;
      location[3] = '\0';

      set_value(location, record_power_elec[i]);
      _delay_ms(25);
    }

    // RECORDS MECH-POWER
    for (int i = 0; i < RECORD_NUMBER; i++) {
      char location[4];

      location[0] = 'n';
      location[1] = '2';
      location[2] = '0' + i;
      location[3] = '\0';

      set_value(location, record_power_mech[i]);
      _delay_ms(25);
    }

    // FROM PAGE 8 to PAGE 9
    cont = 0;
    do {
      uint8_t action = read_value();
      switch (action) {
      case 0xd:
        set_page(9);
        cont = 1;
        break;

      default:
        break;
      }
    } while (!cont);
  }
}

// Optocoupler Functions + Interupts
ISR(INT0_vect) {
  isr0_count++;
  // =======
  //   if (++row_count >= 5) {
  //     sd_logger_close();
  //     while (1)
  //       ; /* halt — card is safe to remove */
  //   }
  // }
  // }
  //
  // // Optocoupler Functions + Interupts
  // ISR(INT0_vect) {
  // >>>>>>> sd_card
  uint16_t current_time0 = TCNT1; // read time for 1st octocoupler
  uint16_t elapsed0 = current_time0 - last_time0;
  last_time0 = current_time0;

  if (elapsed0 > 50) {
    index0 = (index0 + 1) % MAX_BUFFER_SIZE;
    rps0[index0] = (float)TIME_CONSTANT / (elapsed0 * HOLES_PER_REV);
  }
}

ISR(INT1_vect) {
  isr1_count++;
  uint16_t current_time1 = TCNT1; // read time for 2nd octocoupler
  uint16_t elapsed1 = current_time1 - last_time1;
  last_time1 = current_time1;

  if (elapsed1 > 50) {
    index1 = (index1 + 1) % MAX_BUFFER_SIZE;
    rps1[index1] = (float)TIME_CONSTANT / (elapsed1 * HOLES_PER_REV);
  }
}

float average(volatile float rps[], uint8_t index, uint8_t window_size) {
  uint8_t offset = 0;
  float sum = 0.0;

  for (int i = 0; i < window_size; i++) {
    if (i > index) {
      offset = MAX_BUFFER_SIZE;
    }

    sum += rps[offset + index - i];
  }

  return sum / window_size;
}

// End of Optocoupler Functions + Interupts
