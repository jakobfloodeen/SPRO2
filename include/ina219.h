/**
 * ina219.h - Driver for the TI INA219 current/power monitor
 *
 * Target: ATmega328P, AVR-GCC
 * Interface: I2C (TWI hardware peripheral)
 *
 * Datasheet: SBOS448G, Texas Instruments
 */

#ifndef INA219_H
#define INA219_H

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * I2C (TWI) clock
 * Change F_CPU and TWI_FREQ to match your system before compiling.
 * -------------------------------------------------------------------------- */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#ifndef INA219_TWI_FREQ
#define INA219_TWI_FREQ 100000UL   /* 100 kHz standard mode */
#endif

/* --------------------------------------------------------------------------
 * Register addresses
 * -------------------------------------------------------------------------- */
#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNT_V      0x01
#define INA219_REG_BUS_V        0x02
#define INA219_REG_POWER        0x03
#define INA219_REG_CURRENT      0x04
#define INA219_REG_CALIBRATION  0x05

/* --------------------------------------------------------------------------
 * Configuration register bit definitions  (16-bit, MSB first)
 * -------------------------------------------------------------------------- */

/* Bit 15 – RST: writing 1 triggers a full reset (self-clearing) */
#define INA219_CFG_RST          (1u << 15)

/* Bit 13 – BRNG: bus voltage full-scale range */
#define INA219_CFG_BRNG_16V     (0u << 13)
#define INA219_CFG_BRNG_32V     (1u << 13)   /* default */

/* Bits 12:11 – PG: PGA gain / shunt voltage full-scale range */
#define INA219_CFG_PG_40MV      (0u << 11)   /* gain = 1,   FSR = ±40 mV  */
#define INA219_CFG_PG_80MV      (1u << 11)   /* gain = /2,  FSR = ±80 mV  */
#define INA219_CFG_PG_160MV     (2u << 11)   /* gain = /4,  FSR = ±160 mV */
#define INA219_CFG_PG_320MV     (3u << 11)   /* gain = /8,  FSR = ±320 mV (default) */

/* Bits 10:7 – BADC: bus ADC resolution / averaging */
#define INA219_CFG_BADC_9BIT    (0u << 7)    /* 84 µs  */
#define INA219_CFG_BADC_10BIT   (1u << 7)    /* 148 µs */
#define INA219_CFG_BADC_11BIT   (2u << 7)    /* 276 µs */
#define INA219_CFG_BADC_12BIT   (3u << 7)    /* 532 µs (default) */
#define INA219_CFG_BADC_2AVG    (9u << 7)    /* 1.06 ms  */
#define INA219_CFG_BADC_4AVG    (10u << 7)   /* 2.13 ms  */
#define INA219_CFG_BADC_8AVG    (11u << 7)   /* 4.26 ms  */
#define INA219_CFG_BADC_16AVG   (12u << 7)   /* 8.51 ms  */
#define INA219_CFG_BADC_32AVG   (13u << 7)   /* 17.02 ms */
#define INA219_CFG_BADC_64AVG   (14u << 7)   /* 34.05 ms */
#define INA219_CFG_BADC_128AVG  (15u << 7)   /* 68.10 ms */

/* Bits 6:3 – SADC: shunt ADC resolution / averaging (same encoding) */
#define INA219_CFG_SADC_9BIT    (0u << 3)
#define INA219_CFG_SADC_10BIT   (1u << 3)
#define INA219_CFG_SADC_11BIT   (2u << 3)
#define INA219_CFG_SADC_12BIT   (3u << 3)    /* default */
#define INA219_CFG_SADC_2AVG    (9u << 3)
#define INA219_CFG_SADC_4AVG    (10u << 3)
#define INA219_CFG_SADC_8AVG    (11u << 3)
#define INA219_CFG_SADC_16AVG   (12u << 3)
#define INA219_CFG_SADC_32AVG   (13u << 3)
#define INA219_CFG_SADC_64AVG   (14u << 3)
#define INA219_CFG_SADC_128AVG  (15u << 3)

/* Bits 2:0 – MODE: operating mode */
#define INA219_CFG_MODE_POWERDOWN       0u
#define INA219_CFG_MODE_SHUNT_TRIG      1u
#define INA219_CFG_MODE_BUS_TRIG        2u
#define INA219_CFG_MODE_SHUNT_BUS_TRIG  3u
#define INA219_CFG_MODE_ADC_OFF         4u
#define INA219_CFG_MODE_SHUNT_CONT      5u
#define INA219_CFG_MODE_BUS_CONT        6u
#define INA219_CFG_MODE_SHUNT_BUS_CONT  7u   /* default */

/* Convenience: power-on-reset value */
#define INA219_CFG_DEFAULT  0x399Fu

/* --------------------------------------------------------------------------
 * Bus voltage register status bits (bits 1:0)
 * -------------------------------------------------------------------------- */
#define INA219_BUS_CNVR  (1u << 1)   /* conversion ready */
#define INA219_BUS_OVF   (1u << 0)   /* math overflow    */

/* --------------------------------------------------------------------------
 * Slave address table (A1, A0 pin settings → 7-bit address)
 *
 *   A1\A0  GND    VS+    SDA    SCL
 *   GND    0x40   0x41   0x42   0x43
 *   VS+    0x44   0x45   0x46   0x47
 *   SDA    0x48   0x49   0x4A   0x4B
 *   SCL    0x4C   0x4D   0x4E   0x4F
 * -------------------------------------------------------------------------- */
#define INA219_ADDR_GND_GND  0x40
#define INA219_ADDR_GND_VS   0x41
#define INA219_ADDR_GND_SDA  0x42
#define INA219_ADDR_GND_SCL  0x43
#define INA219_ADDR_VS_GND   0x44
#define INA219_ADDR_VS_VS    0x45
#define INA219_ADDR_VS_SDA   0x46
#define INA219_ADDR_VS_SCL   0x47
#define INA219_ADDR_SDA_GND  0x48
#define INA219_ADDR_SDA_VS   0x49
#define INA219_ADDR_SDA_SDA  0x4A
#define INA219_ADDR_SDA_SCL  0x4B
#define INA219_ADDR_SCL_GND  0x4C
#define INA219_ADDR_SCL_VS   0x4D
#define INA219_ADDR_SCL_SDA  0x4E
#define INA219_ADDR_SCL_SCL  0x4F

/* --------------------------------------------------------------------------
 * Error codes
 * -------------------------------------------------------------------------- */
typedef enum {
    INA219_OK            =  0,
    INA219_ERR_BUS       = -1,   /* I2C arbitration / bus error           */
    INA219_ERR_NACK_ADDR = -2,   /* device did not acknowledge address    */
    INA219_ERR_NACK_DATA = -3,   /* device did not acknowledge data byte  */
    INA219_ERR_TIMEOUT   = -4,   /* TWI hardware did not respond          */
    INA219_ERR_OVERFLOW  = -5,   /* OVF flag set in bus voltage register  */
} ina219_err_t;

/* --------------------------------------------------------------------------
 * Device handle
 * -------------------------------------------------------------------------- */
typedef struct {
    uint8_t  addr;          /* 7-bit I2C slave address                    */
    uint16_t config;        /* shadow of the configuration register       */
    /* Calibration / scaling (set by ina219_calibrate) */
    float    current_lsb;   /* amperes per LSB in the Current register    */
    float    power_lsb;     /* watts per LSB in the Power register        */
} ina219_t;

/* --------------------------------------------------------------------------
 * API
 * -------------------------------------------------------------------------- */

/**
 * ina219_init – Initialise the TWI peripheral and reset the INA219.
 *
 * Call once at startup before any other function.
 *
 * @param dev   Pointer to an uninitialised ina219_t struct.
 * @param addr  7-bit I2C address (use INA219_ADDR_* constants).
 * @return      INA219_OK on success, negative error code otherwise.
 */
ina219_err_t ina219_init(ina219_t *dev, uint8_t addr);

/**
 * ina219_configure – Write a custom configuration register value.
 *
 * Build the value using INA219_CFG_* bit definitions, e.g.:
 *   uint16_t cfg = INA219_CFG_BRNG_32V | INA219_CFG_PG_320MV |
 *                  INA219_CFG_BADC_12BIT | INA219_CFG_SADC_12BIT |
 *                  INA219_CFG_MODE_SHUNT_BUS_CONT;
 *
 * @param dev    Device handle.
 * @param config 16-bit configuration word.
 * @return       INA219_OK or a negative error code.
 */
ina219_err_t ina219_configure(ina219_t *dev, uint16_t config);

/**
 * ina219_calibrate – Program the Calibration register for current/power reads.
 *
 * This must be called before ina219_read_current_mA() or ina219_read_power_mW()
 * will return meaningful values.
 *
 * @param dev          Device handle.
 * @param rshunt_ohms  Shunt resistor value in ohms (e.g. 0.1 for 100 mΩ).
 * @param max_amps     Maximum expected current in amperes (sets resolution).
 * @return             INA219_OK or a negative error code.
 */
ina219_err_t ina219_calibrate(ina219_t *dev, float rshunt_ohms, float max_amps);

/**
 * ina219_reset – Perform a software reset (equivalent to power-on reset).
 *
 * All registers return to defaults; calibration must be re-applied.
 *
 * @param dev  Device handle.
 * @return     INA219_OK or a negative error code.
 */
ina219_err_t ina219_reset(ina219_t *dev);

/**
 * ina219_set_mode – Change only the operating mode without altering other bits.
 *
 * @param dev   Device handle.
 * @param mode  INA219_CFG_MODE_* constant.
 * @return      INA219_OK or a negative error code.
 */
ina219_err_t ina219_set_mode(ina219_t *dev, uint8_t mode);

/**
 * ina219_conversion_ready – Poll the CNVR bit in the Bus Voltage register.
 *
 * @param dev   Device handle.
 * @param ready Set to true when data is available.
 * @return      INA219_OK or a negative error code.
 */
ina219_err_t ina219_conversion_ready(ina219_t *dev, bool *ready);

/**
 * ina219_read_shunt_voltage_uV – Read shunt voltage in microvolts.
 *
 * Range depends on PGA setting (±40 mV to ±320 mV). LSB = 10 µV.
 * Negative values indicate reverse current flow.
 *
 * @param dev  Device handle.
 * @param uv   Output: shunt voltage in µV.
 * @return     INA219_OK or a negative error code.
 */
ina219_err_t ina219_read_shunt_voltage_uV(ina219_t *dev, int32_t *uv);

/**
 * ina219_read_bus_voltage_mV – Read bus voltage in millivolts.
 *
 * Measured at IN– relative to GND. LSB = 4 mV.
 *
 * @param dev  Device handle.
 * @param mv   Output: bus voltage in mV.
 * @return     INA219_OK or a negative error code (including INA219_ERR_OVERFLOW).
 */
ina219_err_t ina219_read_bus_voltage_mV(ina219_t *dev, uint16_t *mv);

/**
 * ina219_read_current_mA – Read calculated current in milliamps.
 *
 * Requires ina219_calibrate() to have been called first.
 * Negative values indicate reverse current.
 *
 * @param dev  Device handle.
 * @param ma   Output: current in mA.
 * @return     INA219_OK or a negative error code.
 */
ina219_err_t ina219_read_current_mA(ina219_t *dev, int32_t *ma);

/**
 * ina219_read_power_mW – Read calculated power in milliwatts.
 *
 * Requires ina219_calibrate() to have been called first.
 *
 * @param dev  Device handle.
 * @param mw   Output: power in mW.
 * @return     INA219_OK or a negative error code.
 */
ina219_err_t ina219_read_power_mW(ina219_t *dev, uint32_t *mw);

/**
 * ina219_read_all – Read shunt voltage, bus voltage, current, and power in one pass.
 *
 * Any output pointer may be NULL to skip that measurement.
 *
 * @param dev     Device handle.
 * @param shunt_uV  Shunt voltage in µV (may be NULL).
 * @param bus_mV    Bus voltage in mV   (may be NULL).
 * @param cur_mA    Current in mA       (may be NULL, needs calibration).
 * @param pwr_mW    Power in mW         (may be NULL, needs calibration).
 * @return          INA219_OK or a negative error code.
 */
ina219_err_t ina219_read_all(ina219_t *dev,
                              int32_t  *shunt_uV,
                              uint16_t *bus_mV,
                              int32_t  *cur_mA,
                              uint32_t *pwr_mW);

/* --------------------------------------------------------------------------
 * Low-level register access (useful for advanced configuration)
 * -------------------------------------------------------------------------- */

/**
 * ina219_write_reg – Write a 16-bit register.
 * @param dev    Device handle.
 * @param reg    Register address (INA219_REG_*).
 * @param value  Value to write (big-endian on the wire as per spec).
 */
ina219_err_t ina219_write_reg(ina219_t *dev, uint8_t reg, uint16_t value);

/**
 * ina219_read_reg – Read a 16-bit register.
 * @param dev    Device handle.
 * @param reg    Register address (INA219_REG_*).
 * @param value  Pointer to store the result.
 */
ina219_err_t ina219_read_reg(ina219_t *dev, uint8_t reg, uint16_t *value);

#endif /* INA219_H */
