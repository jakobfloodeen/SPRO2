/*
 * INA219.h
 *
 * Original Author: jpsteph
 * Modified: Removed duplicate #defines
 */

#ifndef INA219_H_
#define INA219_H_

#define INA1ADDR    0b10000000  // I2C address (0x40 pre-shifted left by 1)
#define CONFIG_REG  0x00        // Configuration register
#define CALIB_REG   0x05        // Calibration register
#define SHUNT_REG   0x01        // Shunt voltage register
#define VOLTAGE_REG 0x02        // Bus voltage register
#define POWER_REG   0x03        // Power register
#define CURRENT_REG 0x04        // Current register
#define ILCAL1      1           // Linear current calibration factor

void writeINA(uint8_t adr, uint8_t reg, uint16_t u16data);
uint16_t readINA(uint8_t adr, uint8_t reg);

void INA219_init(void);
void INA219_wakeup(void);
void INA219_sleep(void);

float INA219_get_current(void);
float INA219_get_bus_voltage(void);
float INA219_get_power(void);

#endif /* INA219_H_ */
