#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include "I2C.h"
#include "ina219.h"
#include "uart.h"
#include "motor.h"
#include "adc.h"
//#include "nextion.h"




// Definitions + Storage
#define PRESCALER 1024UL
#define TIME_CONSTANT (F_CPU / PRESCALER) // 15625 ticks/sec
#define HOLES_PER_REV 10
#define MAX_BUFFER_SIZE 20
#define AVERAGING_WINDOW 5
#define BAUDRATE 9600
#define RECORD_NUMBER 10 //keep less than 100

/* OPTO */
volatile uint16_t last_time0 = 0;
volatile uint16_t last_time1 = 0;
volatile float rps0[MAX_BUFFER_SIZE];
volatile float rps1[MAX_BUFFER_SIZE];
volatile uint8_t index0 = 0;
volatile uint8_t index1 = 0;

/* MOTOR */
volatile uint8_t PWM_duty = 100/RECORD_NUMBER;// Starting value 

/* STORAGE */
volatile float record_voltage[RECORD_NUMBER]; //records to hold "X", for "Y" values
volatile float record_current[RECORD_NUMBER];
volatile float record_power_elec[RECORD_NUMBER];
volatile float record_power_mech[RECORD_NUMBER];
volatile float record_RPM[RECORD_NUMBER];
volatile float record_torque[RECORD_NUMBER];
volatile float record_force[RECORD_NUMBER];
volatile float record_duty[RECORD_NUMBER];

volatile uint16_t isr0_count = 0;
volatile uint16_t isr1_count = 0;






float average (volatile float*, uint8_t, uint8_t);

// End of Definitions + storage

// Main

int main(void)
{
    USART_Init(BAUDRATE);
    UART_EnablePrintf();
    TWIInit(100000); // (100kHz)
    INA219_init();
    pwm1_init();
    adc_init();
    //nextion_init();
    

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

    // Start of Nextion Main
 




    while(1) //increases duty cycle for PWM by 1 each second and measures RPM, Voltage, Current and pressure on FSR
    {      
      for(int i=0; i<RECORD_NUMBER; i++){        
        // pwm1_set_duty(PWM_duty*i);// set duty cycle
        // record_duty[i] = (PWM_duty*i); // %
        _delay_ms(500);
        // record_voltage[i] = INA219_get_bus_voltage(); //V  
        // record_current[i] = INA219_get_current(); //mA
        // record_power_elec[i] = INA219_get_power(); //mW
        record_RPM[i] = (average(rps0,index0,AVERAGING_WINDOW) + average(rps1,index1,AVERAGING_WINDOW))/2;// RPM (average of both optos)
        printf("%f\n",record_RPM[i]);
        printf("ISR0: %u, ISR1: %u\n", isr0_count, isr1_count);
         //record_force[i] = adc_read(); //value from 0 - (2^16)-1 (max is 5V). 1 = 0.07629394531mV, (2^16)-1 = 5000mV
       
        //printf("%.4f\n", adc_to_voltage(adc_read()));

        // record_torque[i] = record_force[i] * length;
        // record_power_mech[i] = record_torque[i] * ((record_RPM[i]*2*3.1415)/60)

      }
    }
}


 // Optocoupler Functions + Interupts
ISR(INT0_vect)
{
  isr0_count++;
  uint16_t current_time0 = TCNT1; // read time for 1st octocoupler
  uint16_t elapsed0 = current_time0 - last_time0;
  last_time0 = current_time0;

  if (elapsed0 > 50)
  {
    index0 = (index0 + 1) % MAX_BUFFER_SIZE;
    rps0[index0] = (float)TIME_CONSTANT / (elapsed0 * HOLES_PER_REV);
  }
}

ISR(INT1_vect)
{
  isr1_count++;
  uint16_t current_time1 = TCNT1; // read time for 2nd octocoupler
  uint16_t elapsed1 = current_time1 - last_time1;
  last_time1 = current_time1;


  
  if (elapsed1 > 50)
  {
    index1 = (index1 + 1) % MAX_BUFFER_SIZE;
    rps1[index1] = (float)TIME_CONSTANT / (elapsed1 * HOLES_PER_REV);
  }
}

float average (volatile float rps[], uint8_t index, uint8_t window_size)
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