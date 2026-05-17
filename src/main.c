/*
 * WHAT WE HAVE SO FAR
 * - voltage, current, power, RPS
 * - can easily calculate torque, angular velocity, and mechanical power (look at graph on typst)
 * 
 * WHAT WE NEED
 * - interface with Nextion Display
 * - Efficency
 * - Interact with motor driver
 * - Saftey check
 * - Stop interrupt
 * - Average of all runtime values based off of total time elapsed
 *
 *
 * main.c
 * Connections:
 *   Optocoupler 1 output -> PD2 (INT0)
 *   Optocoupler 2 output -> PD3 (INT1)
 *   INA219 SDA           -> PC4 (SDA)
 *   INA219 SCL           -> PC5 (SCL)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include "I2C.h"
#include "INA219.h"
#include "usart.h"


// Configuration


#define MAX_BUFFER_SIZE  8      // Rolling average window storage size
#define WINDOW_SIZE      4      // Number of samples to average over
#define HOLES_PER_REV    1      // Number of holes/slots per full wheel revolution
#define TIME_CONSTANT    (F_CPU / 1024.0f)  // Timer1 ticks per second (prescaler 1024)
#define RSHUNT           0.1f  // Shunt resistor value in ohms


// Optocoupler state (volatile - modified inside ISRs)


static volatile uint16_t last_time0 = 0;
static volatile uint16_t last_time1 = 0;
static volatile uint8_t  index0     = 0;
static volatile uint8_t  index1     = 0;
static volatile float    rps0[MAX_BUFFER_SIZE];
static volatile float    rps1[MAX_BUFFER_SIZE];


// ISR: Optocoupler 1 (INT0 - PD2)


ISR(INT0_vect)
{
    uint16_t current_time0 = TCNT1;
    uint16_t elapsed0      = current_time0 - last_time0;
    last_time0             = current_time0;

    if (elapsed0 > 0)
    {
        index0       = (index0 + 1) % MAX_BUFFER_SIZE;
        rps0[index0] = (float)TIME_CONSTANT / (elapsed0 * HOLES_PER_REV);
    }
}


// ISR: Optocoupler 2 (INT1 - PD3)


ISR(INT1_vect)
{
    uint16_t current_time1 = TCNT1;
    uint16_t elapsed1      = current_time1 - last_time1;
    last_time1             = current_time1;

    if (elapsed1 > 0)
    {
        index1       = (index1 + 1) % MAX_BUFFER_SIZE;
        rps1[index1] = (float)TIME_CONSTANT / (elapsed1 * HOLES_PER_REV);
    }
}


// Wheel speed: rolling average over the last window_size samples (RPS)


float average(volatile float rps[], uint8_t index, uint8_t window_size)
{
    float   sum    = 0.0f;
    uint8_t offset = 0;

    for (int i = 0; i < window_size; i++)
    {
        offset = (i > index) ? MAX_BUFFER_SIZE : 0;
        sum   += rps[offset + index - i];
    }

    return sum / window_size;
}

/*
 * get_wheel_speed()
 *
 * Averages both optocoupler RPS readings and returns the combined
 * average in revolutions per second.
 */
float get_wheel_speed(void)
{
    float avg0 = average(rps0, index0, WINDOW_SIZE);
    float avg1 = average(rps1, index1, WINDOW_SIZE);
    return (avg0 + avg1) / 2.0f;
}


// INA219: Voltage, current, power measurement struct + function


typedef struct {
    float voltage;  // Bus voltage in V
    float current;  // Current in mA
    float power;    // Power in mW
} INA219_Reading;

/*
 * get_power_reading()
 *
 * Reads voltage (V), current (mA), and power (mW) from the INA219
 * and returns them packed in an INA219_Reading struct.
 */
INA219_Reading get_power_reading(void)
{
    INA219_Reading reading;
    reading.voltage = INA219_get_bus_voltage();
    reading.current = INA219_get_current(RSHUNT);
    reading.power   = INA219_get_power(RSHUNT);
    return reading;
}


// Main


int main(void)
{
    // --- Timer1: Normal mode, prescaler 1024 ---
    TCCR1A = 0x00;
    TCCR1B = (1 << CS12) | (1 << CS10);

    // --- External interrupts: rising edge on INT0 and INT1 ---
    EICRA = (1 << ISC00) | (1 << ISC01) |  // INT0 rising edge
            (1 << ISC10) | (1 << ISC11);    // INT1 rising edge
    EIMSK = (1 << INT0)  | (1 << INT1);

    // --- I2C + INA219 ---
    TWIInit(300000);   // 300kHz I2C clock
    INA219_init();

    sei();  // Enable global interrupts

    while (1)
    {
        float          speed   = get_wheel_speed();
        INA219_Reading reading = get_power_reading();

        // speed           -> rps
        // reading.voltage -> v
        // reading.current -> mA
        // reading.power   -> mW
        printf("Voltage (V): %f\n", reading.voltage);
        printf("Current (mA): %f\n", reading.current);
        printf("Power (mW): %f\n", reading.power);
        printf("Speed (RPS): %f\n", speed);
        
        
    }
}
