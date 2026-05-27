#include "adc.h"
#include <avr/io.h>

void adc_init(void){
    // Start of ADC Main
    ADMUX = (0 << REFS1) | (1 << REFS0) | (0 << ADLAR) | (0 << MUX3) | (0 << MUX2) | (0 << MUX1) | (0 << MUX0); 
    // Reference Selection Bits, these bits select the voltage reference for the ADC. AVCC with external capacitor at AREF pin
    // The value of these bits selects which analog inputs are connected to the ADC, MUX0...3.
    // ADLAR ADC Left Adjust Result. The ADLAR bit affects the presentation of the ADC conversion result in the ADC data register.

    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // ADC Control and Status Register A
    // Writing this bit to one enables the ADC.
    // Prescaler Selections ADPS0...2 set prescaler 128

    ADCSRB = (0 << ADTS2) | (0 << ADTS1) | (0 << ADTS0); // ADC Control and Status Register B
    //ADC Auto Trigger Source ADTS0..2 set to free running.

    // End of ADC Main
}

uint16_t adc_read(void){
    ADCSRA |= (1<<ADSC);

    while ((ADCSRA & (1 << ADSC )));

    return ((uint16_t)ADCH << 8) | ADCL;
}