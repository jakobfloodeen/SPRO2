/*
 * I2C.c
 *
 * Author: jpsteph
 */

#include <avr/io.h>
#include "usart.h"
#include "I2C.h"

void TWIInit(uint32_t speed) {
    uint32_t gen_t;
    TWSR = 0x00;
    gen_t = (((F_CPU / speed) - 16) / 2) & 0xFF;
    TWBR = gen_t & 0xFF;
    TWCR = (1 << TWEN);
}

void TWIStart(void) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while ((TWCR & (1 << TWINT)) == 0);
}

void TWIStop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void TWIWrite(uint8_t u8data) {
    TWDR = u8data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while ((TWCR & (1 << TWINT)) == 0);
}

void TWIWriteACK(uint8_t u8data) {
    TWDR = u8data;
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    while ((TWCR & (1 << TWINT)) == 0);
}

uint8_t TWIReadACK(void) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    while ((TWCR & (1 << TWINT)) == 0);
    return TWDR;
}

uint8_t TWIReadNACK(void) {
    TWCR = (1 << TWINT) | (1 << TWEN);
    while ((TWCR & (1 << TWINT)) == 0);
    return TWDR;
}

uint8_t TWIGetStatus(void) {
    return TWSR & 0xF8;
}
