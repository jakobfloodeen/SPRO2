/**
 * ina219.c – Driver implementation for the TI INA219 current/power monitor
 *
 * Target:    ATmega328P, AVR-GCC
 * Interface: Hardware TWI (I2C) peripheral
 *
 * TWI register map used (avr/twi.h mnemonics):
 *   TWBR  – Bit-rate register
 *   TWSR  – Status register (includes prescaler bits)
 *   TWCR  – Control register
 *   TWDR  – Data register
 *   TWAR  – (not used – we are always master)
 */

#include "ina219.h"

#include <avr/io.h>
#include <util/twi.h>   /* TW_* status codes */
#include <util/delay.h>
#include <stddef.h>

/* --------------------------------------------------------------------------
 * Internal TWI helpers
 * -------------------------------------------------------------------------- */

/* Timeout loop count – roughly proportional to cycles waiting for TWINT */
#define TWI_TIMEOUT_COUNT  50000u

/** Send a TWI START condition and wait for TWINT. */
static ina219_err_t twi_start(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);

    uint32_t cnt = TWI_TIMEOUT_COUNT;
    while (!(TWCR & (1 << TWINT))) {
        if (--cnt == 0) return INA219_ERR_TIMEOUT;
    }

    uint8_t status = TW_STATUS;
    if (status != TW_START && status != TW_REP_START)
        return INA219_ERR_BUS;

    return INA219_OK;
}

/** Send a TWI STOP condition. */
static void twi_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    /* No need to poll TWINT after STOP */
}

/**
 * Write a single byte onto the TWI bus and wait for acknowledgement.
 * Returns INA219_ERR_NACK_ADDR if the status is SLA+W not acknowledged,
 *         INA219_ERR_NACK_DATA if a data byte was not acknowledged.
 */
static ina219_err_t twi_write_byte(uint8_t byte, bool is_address)
{
    TWDR = byte;
    TWCR = (1 << TWINT) | (1 << TWEN);

    uint32_t cnt = TWI_TIMEOUT_COUNT;
    while (!(TWCR & (1 << TWINT))) {
        if (--cnt == 0) return INA219_ERR_TIMEOUT;
    }

    uint8_t status = TW_STATUS;
    if (is_address) {
        if (status != TW_MT_SLA_ACK && status != TW_MR_SLA_ACK)
            return INA219_ERR_NACK_ADDR;
    } else {
        if (status != TW_MT_DATA_ACK)
            return INA219_ERR_NACK_DATA;
    }

    return INA219_OK;
}

/**
 * Read a byte from the TWI bus.
 *
 * @param ack  true  = send ACK after byte (more bytes to follow)
 *             false = send NACK (last byte)
 */
static ina219_err_t twi_read_byte(uint8_t *byte, bool ack)
{
    if (ack)
        TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    else
        TWCR = (1 << TWINT) | (1 << TWEN);

    uint32_t cnt = TWI_TIMEOUT_COUNT;
    while (!(TWCR & (1 << TWINT))) {
        if (--cnt == 0) return INA219_ERR_TIMEOUT;
    }

    uint8_t status = TW_STATUS;
    if (ack  && status != TW_MR_DATA_ACK)  return INA219_ERR_BUS;
    if (!ack && status != TW_MR_DATA_NACK) return INA219_ERR_BUS;

    *byte = TWDR;
    return INA219_OK;
}

/* --------------------------------------------------------------------------
 * Register access
 * -------------------------------------------------------------------------- */

ina219_err_t ina219_write_reg(ina219_t *dev, uint8_t reg, uint16_t value)
{
    ina219_err_t err;

    if ((err = twi_start()) != INA219_OK)               { twi_stop(); return err; }
    if ((err = twi_write_byte((uint8_t)(dev->addr << 1),
                               true)) != INA219_OK)     { twi_stop(); return err; }
    if ((err = twi_write_byte(reg, false)) != INA219_OK){ twi_stop(); return err; }
    /* MSB first (big-endian), as required by the INA219 */
    if ((err = twi_write_byte((uint8_t)(value >> 8),
                               false)) != INA219_OK)    { twi_stop(); return err; }
    if ((err = twi_write_byte((uint8_t)(value & 0xFF),
                               false)) != INA219_OK)    { twi_stop(); return err; }
    twi_stop();
    return INA219_OK;
}

ina219_err_t ina219_read_reg(ina219_t *dev, uint8_t reg, uint16_t *value)
{
    ina219_err_t err;
    uint8_t msb, lsb;

    /* Set register pointer */
    if ((err = twi_start()) != INA219_OK)               { twi_stop(); return err; }
    if ((err = twi_write_byte((uint8_t)(dev->addr << 1),
                               true)) != INA219_OK)     { twi_stop(); return err; }
    if ((err = twi_write_byte(reg, false)) != INA219_OK){ twi_stop(); return err; }

    /* Repeated START, then read two bytes */
    if ((err = twi_start()) != INA219_OK)               { twi_stop(); return err; }
    if ((err = twi_write_byte((uint8_t)((dev->addr << 1) | 0x01),
                               true)) != INA219_OK)     { twi_stop(); return err; }
    if ((err = twi_read_byte(&msb, true))  != INA219_OK){ twi_stop(); return err; }
    if ((err = twi_read_byte(&lsb, false)) != INA219_OK){ twi_stop(); return err; }
    twi_stop();

    *value = ((uint16_t)msb << 8) | lsb;
    return INA219_OK;
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

ina219_err_t ina219_init(ina219_t *dev, uint8_t addr)
{
    if (dev == NULL) return INA219_ERR_BUS;

    dev->addr        = addr;
    dev->config      = INA219_CFG_DEFAULT;
    dev->current_lsb = 0.0f;
    dev->power_lsb   = 0.0f;

    /* Configure TWI bit-rate.
     * TWBR = (F_CPU / TWI_FREQ - 16) / (2 * prescaler)
     * We use prescaler = 1 (TWPS bits = 00). */
    TWSR = 0x00;  /* prescaler = 1 */
    TWBR = (uint8_t)((F_CPU / INA219_TWI_FREQ - 16UL) / 2UL);
    TWCR = (1 << TWEN);

    /* Issue a software reset so the device starts from a known state */
    return ina219_reset(dev);
}

ina219_err_t ina219_reset(ina219_t *dev)
{
    ina219_err_t err = ina219_write_reg(dev, INA219_REG_CONFIG,
                                        INA219_CFG_DEFAULT | INA219_CFG_RST);
    if (err != INA219_OK) return err;

    /* The datasheet notes registers are volatile; restore sensible defaults */
    dev->config      = INA219_CFG_DEFAULT;
    dev->current_lsb = 0.0f;
    dev->power_lsb   = 0.0f;

    _delay_ms(1); /* Allow device to finish reset */
    return INA219_OK;
}

ina219_err_t ina219_configure(ina219_t *dev, uint16_t config)
{
    ina219_err_t err = ina219_write_reg(dev, INA219_REG_CONFIG, config);
    if (err == INA219_OK)
        dev->config = config;
    return err;
}

ina219_err_t ina219_set_mode(ina219_t *dev, uint8_t mode)
{
    uint16_t config = (dev->config & ~0x0007u) | (mode & 0x0007u);
    return ina219_configure(dev, config);
}

ina219_err_t ina219_calibrate(ina219_t *dev, float rshunt_ohms, float max_amps)
{
    /*
     * From the datasheet (Section 8.5.1):
     *
     *   Current_LSB = max_amps / 2^15
     *   Cal         = trunc(0.04096 / (Current_LSB * R_SHUNT))
     *
     * We round Current_LSB up to preserve resolution, then re-derive the
     * actual LSB from the truncated Cal value so that conversions are exact.
     */
    dev->current_lsb = max_amps / 32768.0f;            /* A/LSB */
    dev->power_lsb   = dev->current_lsb * 20.0f;       /* W/LSB */

    uint16_t cal = (uint16_t)(0.04096f / (dev->current_lsb * rshunt_ohms));

    return ina219_write_reg(dev, INA219_REG_CALIBRATION, cal);
}

ina219_err_t ina219_conversion_ready(ina219_t *dev, bool *ready)
{
    uint16_t raw;
    ina219_err_t err = ina219_read_reg(dev, INA219_REG_BUS_V, &raw);
    if (err != INA219_OK) return err;

    *ready = (raw & INA219_BUS_CNVR) != 0;
    return INA219_OK;
}

ina219_err_t ina219_read_shunt_voltage_uV(ina219_t *dev, int32_t *uv)
{
    uint16_t raw;
    ina219_err_t err = ina219_read_reg(dev, INA219_REG_SHUNT_V, &raw);
    if (err != INA219_OK) return err;

    /*
     * The register is a 16-bit two's-complement value.
     * LSB = 10 µV regardless of PGA setting.
     * Cast through int16_t to handle the sign correctly.
     */
    *uv = (int32_t)(int16_t)raw * 10;
    return INA219_OK;
}

ina219_err_t ina219_read_bus_voltage_mV(ina219_t *dev, uint16_t *mv)
{
    uint16_t raw;
    ina219_err_t err = ina219_read_reg(dev, INA219_REG_BUS_V, &raw);
    if (err != INA219_OK) return err;

    if (raw & INA219_BUS_OVF)
        return INA219_ERR_OVERFLOW;

    /*
     * BD12:BD0 are in bits 15:3.  Shift right by 3, multiply by 4 mV LSB.
     */
    *mv = (uint16_t)((raw >> 3) * 4u);
    return INA219_OK;
}

ina219_err_t ina219_read_current_mA(ina219_t *dev, int32_t *ma)
{
    uint16_t raw;
    ina219_err_t err = ina219_read_reg(dev, INA219_REG_CURRENT, &raw);
    if (err != INA219_OK) return err;

    /*
     * Two's-complement 16-bit value.
     * Actual current (A) = raw * current_lsb.
     * We return milliamps.
     */
    int16_t signed_raw = (int16_t)raw;
    *ma = (int32_t)(signed_raw * (dev->current_lsb * 1000.0f));
    return INA219_OK;
}

ina219_err_t ina219_read_power_mW(ina219_t *dev, uint32_t *mw)
{
    uint16_t raw;
    ina219_err_t err = ina219_read_reg(dev, INA219_REG_POWER, &raw);
    if (err != INA219_OK) return err;

    /*
     * Power register is unsigned 16-bit.
     * Actual power (W) = raw * power_lsb  (power_lsb = 20 × current_lsb).
     * We return milliwatts.
     */
    *mw = (uint32_t)((float)raw * (dev->power_lsb * 1000.0f));
    return INA219_OK;
}

ina219_err_t ina219_read_all(ina219_t *dev,
                              int32_t  *shunt_uV,
                              uint16_t *bus_mV,
                              int32_t  *cur_mA,
                              uint32_t *pwr_mW)
{
    ina219_err_t err;

    if (shunt_uV) {
        if ((err = ina219_read_shunt_voltage_uV(dev, shunt_uV)) != INA219_OK)
            return err;
    }
    if (bus_mV) {
        if ((err = ina219_read_bus_voltage_mV(dev, bus_mV)) != INA219_OK)
            return err;
    }
    if (cur_mA) {
        if ((err = ina219_read_current_mA(dev, cur_mA)) != INA219_OK)
            return err;
    }
    if (pwr_mW) {
        if ((err = ina219_read_power_mW(dev, pwr_mW)) != INA219_OK)
            return err;
    }

    return INA219_OK;
}
