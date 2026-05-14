/**
 * example.c – Usage example for the INA219 driver on an ATmega328P
 *
 * Hardware assumptions:
 *   - 16 MHz crystal
 *   - INA219 with A0=GND, A1=GND  → address 0x40
 *   - 100 mΩ (0.1 Ω) shunt resistor
 *   - Maximum expected current: 3.2 A
 *   - UART at 9600 baud for debug output (TX = PD1)
 *
 * Compile with avr-gcc, e.g.:
 *   avr-gcc -mmcu=atmega328p -DF_CPU=16000000UL -Os \
 *           -o example.elf example.c ina219.c
 *   avr-objcopy -O ihex example.elf example.hex
 */

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "ina219.h"
#include <avr/interrupt.h>

/* --------------------------------------------------------------------------
 * Minimal UART for debug output (9600 baud, 8N1)
 * -------------------------------------------------------------------------- */
#define BAUD      9600UL
#define UBRR_VAL  ((F_CPU / (16UL * BAUD)) - 1)

// Optocoupler Definitions + Storage
#define PRESCALER 1024UL
#define TIME_CONSTANT (F_CPU / PRESCALER) // 15625 ticks/sec
#define HOLES_PER_REV 10
#define MAX_BUFFER_SIZE 20
#define Averaging_Window 5

volatile uint16_t last_time0 = 0;
volatile uint16_t last_time1 = 0;
volatile float rps0[MAX_BUFFER_SIZE];
volatile float rps1[MAX_BUFFER_SIZE];
volatile uint8_t index0 = 0;
volatile uint8_t index1 = 0;

float average (float*, uint8_t, uint8_t);

// End of optocoupler Definitions + storage

static void uart_init(void)
{
    UBRR0H = (uint8_t)(UBRR_VAL >> 8);
    UBRR0L = (uint8_t)(UBRR_VAL);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  /* 8 data bits */
}

static void uart_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = (uint8_t)c;
}

static int uart_putchar(char c, FILE *stream)
{
    (void)stream;
    if (c == '\n') uart_putc('\r');
    uart_putc(c);
    return 0;
}

static FILE uart_stdout = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

/* --------------------------------------------------------------------------
 * Main
 * -------------------------------------------------------------------------- */
int main(void)
{
    uart_init();
    stdout = &uart_stdout;

    printf("INA219 driver example\n");

    /* ------------------------------------------------------------------
     * Initialise the device handle and the TWI peripheral
     * ------------------------------------------------------------------ */
    ina219_t sensor;
    ina219_err_t err = ina219_init(&sensor, INA219_ADDR_GND_GND);
    if (err != INA219_OK) {
        printf("ina219_init failed: %d\n", err);
        while (1);
    }

    /* ------------------------------------------------------------------
     * Optional: custom configuration
     *   - 32 V bus range
     *   - ±320 mV shunt range (PGA = /8, suitable for 100 mΩ at 3.2 A)
     *   - 12-bit ADC, 128-sample averaging (68 ms per conversion)
     *   - Continuous shunt + bus measurement
     * ------------------------------------------------------------------ */
    uint16_t cfg = INA219_CFG_BRNG_32V
                 | INA219_CFG_PG_320MV
                 | INA219_CFG_BADC_128AVG
                 | INA219_CFG_SADC_128AVG
                 | INA219_CFG_MODE_SHUNT_BUS_CONT;

    err = ina219_configure(&sensor, cfg);
    if (err != INA219_OK) {
        printf("ina219_configure failed: %d\n", err);
        while (1);
    }

    /* ------------------------------------------------------------------
     * Calibrate for a 100 mΩ shunt, max 3.2 A
     * ------------------------------------------------------------------ */
    err = ina219_calibrate(&sensor, 0.1f, 3.2f);
    if (err != INA219_OK) {
        printf("ina219_calibrate failed: %d\n", err);
        while (1);
    }

    printf("Calibration done. Entering measurement loop...\n\n");


// Optocoupler Main
  // --- Initilizations some may not be needed ---
  //io_redirect(); // redirect input and output to the communication
  //usart_init();
  // i2c_init(); // initialize I2C communication

  // --- Timer/counter1 initilization --- 64 ticks per second
  // Set the Timer1 Mode to Normal
  TCCR1A = 0x00;
  TCCR1B = (1 << CS12) | (1 << CS10); // prescaler 1024, noise canceling, rising edge

  // External Interrupt Control Register A
  EICRA = (1 << ISC00) | (1 << ISC01) | (1 << ISC10) | (1 << ISC11); // Rising edge, The rising edge of INT1 generates an interrupt request.
  // External Interrupt Mask Register
  EIMSK = (1 << INT0) | (1 << INT1); // External Interrupt Request 0 and 1 Enabled

  sei(); 
  // End of Optocoupler Main

    /* ------------------------------------------------------------------
     * Measurement loop
     * ------------------------------------------------------------------ */
    while (1) {
        /* Wait for a fresh conversion */
        bool ready = false;
        uint8_t retries = 100;
        do {
            _delay_ms(1);
            err = ina219_conversion_ready(&sensor, &ready);
            if (err != INA219_OK) {
                printf("conversion_ready error: %d\n", err);
                break;
            }
        } while (!ready && --retries);

        /* Read all measurements in one call */
        int32_t  shunt_uV = 0;
        uint16_t bus_mV   = 0;
        int32_t  cur_mA   = 0;
        uint32_t pwr_mW   = 0;

        err = ina219_read_all(&sensor, &shunt_uV, &bus_mV, &cur_mA, &pwr_mW);
        if (err == INA219_ERR_OVERFLOW) {
            printf("WARNING: Math overflow – check calibration / shunt range\n");
        } else if (err != INA219_OK) {
            printf("ina219_read_all error: %d\n", err);
        } else {
            printf("Shunt: %ld uV  Bus: %u mV  Current: %ld mA  Power: %lu mW\n",
                   shunt_uV, bus_mV, cur_mA, pwr_mW);
        }

        _delay_ms(50);
    }

    return 0; /* Never reached */
}

// Optocoupler Functions + Interupts
ISR(INT0_vect)
{
  uint16_t current_time0 = TCNT1; // read time for 1st octocoupler
  uint16_t elapsed0 = current_time0 - last_time0;
  last_time0 = current_time0;

  if (elapsed0 > 0)
  {
    index0 = (index0 + 1) % MAX_BUFFER_SIZE;
    rps0[index0] = (float)TIME_CONSTANT / (elapsed0 * HOLES_PER_REV);
  }
}

ISR(INT1_vect)
{
  uint16_t current_time1 = TCNT1; // read time for 2nd octocoupler
  uint16_t elapsed1 = current_time1 - last_time1;
  last_time1 = current_time1;

  if (elapsed1 > 0)
  {
    index1 = (index1 + 1) % MAX_BUFFER_SIZE;
    rps1[index1] = (float)TIME_CONSTANT / (elapsed1 * HOLES_PER_REV);
  }
}

float average (float rps[], uint8_t index, uint8_t window_size)
{
    uint8_t offset = 0;
    float sum = 0.0;

    for (int i = 0; i < window_size; i++)
    {
        if (i > index)
        {
            offset = MAX_BUFFER_SIZE;
        }

        sum += rps[offset + index - i];
    }

    return sum/window_size;
}

//End of Optocoupler Functions + Interupts