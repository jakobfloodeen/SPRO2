#include "I2C.h"
#include "ina219.h"
#include "motor.h"
#include "sd_logger.h"
#include "uart.h"
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>

// Definitions + Storage
#define PRESCALER 1024UL
#define TIME_CONSTANT (F_CPU / PRESCALER) // 15625 ticks/sec
#define HOLES_PER_REV 10
#define MAX_BUFFER_SIZE 20
#define Averaging_Window 5
#define BAUDRATE 9600

volatile uint16_t last_time0 = 0;
volatile uint16_t last_time1 = 0;
volatile float rps0[MAX_BUFFER_SIZE];
volatile float rps1[MAX_BUFFER_SIZE];
volatile uint8_t index0 = 0;
volatile uint8_t index1 = 0;
volatile uint8_t PWM_duty = 0;

float average(float *, uint8_t, uint8_t);

// End of Definitions + storage

void __attribute__((naked, section(".init3"))) stack_init(void) { SP = RAMEND; }

// Main
int main(void) {
  sd_logger_init("a,b,c");
  _delay_ms(1000);

  uint8_t row_count = 0;
  static char row[] = "1,2,3";

  while (1) {
    PORTB |= (1 << PB5);
    _delay_ms(500);
    PORTB &= ~(1 << PB5);
    _delay_ms(500);
    PORTB |= (1 << PB5);
    _delay_ms(500);
    PORTB &= ~(1 << PB5);
    _delay_ms(500);
    PORTB |= (1 << PB5);
    _delay_ms(500);
    PORTB &= ~(1 << PB5);
    _delay_ms(500);

    sd_logger_write(row);
    _delay_ms(1000);

    if (++row_count >= 5) {
      sd_logger_close();
      while (1)
        ; /* halt — card is safe to remove */
    }
  }
}

// Optocoupler Functions + Interupts
ISR(INT0_vect) {
  uint16_t current_time0 = TCNT1; // read time for 1st octocoupler
  uint16_t elapsed0 = current_time0 - last_time0;
  last_time0 = current_time0;

  if (elapsed0 > 0) {
    index0 = (index0 + 1) % MAX_BUFFER_SIZE;
    rps0[index0] = (float)TIME_CONSTANT / (elapsed0 * HOLES_PER_REV);
  }
}

ISR(INT1_vect) {
  uint16_t current_time1 = TCNT1; // read time for 2nd octocoupler
  uint16_t elapsed1 = current_time1 - last_time1;
  last_time1 = current_time1;

  if (elapsed1 > 0) {
    index1 = (index1 + 1) % MAX_BUFFER_SIZE;
    rps1[index1] = (float)TIME_CONSTANT / (elapsed1 * HOLES_PER_REV);
  }
}

float average(float rps[], uint8_t index, uint8_t window_size) {
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
