/*
 * INA219.c
 *
 * Original Author: jpsteph
 * Fixes:
 *   - writeINA(): corrected low-byte mask from (u16data | 0xFF) to (u16data & 0xFF)
 *   - get_bus_voltage() renamed to INA219_get_bus_voltage() for consistency
 * Added:
 *   - INA219_get_power(): returns power in mW (V * I)
 */

#include "I2C.h"
#include "INA219.h"

#define INA1CONFIG   0b0010011001100111  // Config register value per datasheet
#define INACALIB     5120                // Calibration value for R_SHUNT = 0.1 ohm

#define INASETMODE   0xFFF8
#define INACONTMODE  0x0007
#define INASLEEPMODE 0x0000

void writeINA(uint8_t adr, uint8_t reg, uint16_t u16data) {
    TWIStart();
    TWIWrite(adr);
    TWIWrite(reg);
    TWIWrite((uint8_t)(u16data >> 8));
    TWIWrite((uint8_t)(u16data & 0xFF));  // BUG FIX: was (u16data | 0xFF), always wrote 0xFF
    TWIStop();
}

uint16_t readINA(uint8_t adr, uint8_t reg) {
    uint16_t result;

    // Set register pointer
    TWIStart();
    TWIWrite(adr);
    TWIWrite(reg);
    TWIStop();

    // Read 2 bytes
    TWIStart();
    TWIWrite(adr | 0x01);
    result  = ((uint16_t)TWIReadACK())  << 8;
    result |=  (uint16_t)TWIReadNACK();
    TWIStop();

    return result;
}

void INA219_init(void) {
    writeINA(INA1ADDR, CONFIG_REG, INA1CONFIG);
    writeINA(INA1ADDR, CALIB_REG,  ILCAL1 * INACALIB);
}

void INA219_wakeup(void) {
    writeINA(INA1ADDR, CONFIG_REG, (INA1CONFIG & INASETMODE) | INACONTMODE);
}

void INA219_sleep(void) {
    writeINA(INA1ADDR, CONFIG_REG, (INA1CONFIG & INASETMODE) | INASLEEPMODE);
}

// Returns current in mA
float INA219_get_current(float shunt_resistance) {
    uint16_t shuntVoltageReg = readINA(INA1ADDR, SHUNT_REG);
    float shuntVoltage;

    if (shuntVoltageReg & 0x8000) {
        // Two's complement: negative shunt voltage
        shuntVoltage = (float)(int16_t)((~shuntVoltageReg) + 1) * -1.0f;
    } else {
        shuntVoltage = (float)shuntVoltageReg;
    }

    shuntVoltage /= 100.0f;  // Convert to mV (LSB = 10uV = 0.01mV)

    return shuntVoltage / shunt_resistance;  // I (mA) = V (mV) / R (ohm)
}

// Returns bus voltage in V
float INA219_get_bus_voltage(void) {
    uint16_t busVoltageReg = readINA(INA1ADDR, VOLTAGE_REG);
    float busVoltage;

    if (busVoltageReg & 0x01) {
        // OVF bit set - overflow
        busVoltage = 0xFFFF;
    } else {
        // Bits [15:3], LSB = 4mV
        busVoltage = (float)(busVoltageReg >> 3) * 4.0f;
    }

    return busVoltage / 1000.0f;  // Convert mV to V
}

// Returns power in mW (P = V * I)
float INA219_get_power(float shunt_resistance) {
    float voltage = INA219_get_bus_voltage();   // V
    float current = INA219_get_current(shunt_resistance);  // mA
    return voltage * current;  // mW
}
