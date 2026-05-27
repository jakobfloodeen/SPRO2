#ifndef FSR_ADC
#define FSR_ADC

#include <stdint.h>

void adc_init(void);
uint16_t adc_read(void);

#endif