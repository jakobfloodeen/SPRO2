#include "diskio.h"
#include <avr/io.h>
#include <util/delay.h>

#define DDR_SPI DDRB
#define PORT_SPI PORTB
#define DD_MOSI DDB3
#define DD_MISO DDB4
#define DD_SCK DDB5
#define DD_CS DDB2

#define CS_LOW() (PORT_SPI &= ~(1 << DD_CS))
#define CS_HIGH() (PORT_SPI |= (1 << DD_CS))

static uint8_t spi_transfer(uint8_t data) {
  SPDR = data;
  while (!(SPSR & (1 << SPIF)))
    ;
  return SPDR;
}

static void spi_init(void) {
  DDR_SPI |= (1 << DD_MOSI) | (1 << DD_SCK) | (1 << DD_CS);
  DDR_SPI &= ~(1 << DD_MISO);
  PORT_SPI |= (1 << DD_MISO); /* pull-up on MISO */
  CS_HIGH();
  SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0); /* /128 */
  SPSR &= ~(1 << SPI2X);
}

static void spi_fast(void) {
  SPCR = (1 << SPE) | (1 << MSTR);
  SPSR |= (1 << SPI2X); /* /2 */
}

/* Send command, return R1 response. CS must be LOW before calling. */
static uint8_t send_cmd(uint8_t cmd, uint32_t arg) {
  uint8_t crc = 0x95;
  if (cmd == 8)
    crc = 0x87;

  spi_transfer(0xFF); /* guard byte */
  spi_transfer(0x40 | cmd);
  spi_transfer((arg >> 24) & 0xFF);
  spi_transfer((arg >> 16) & 0xFF);
  spi_transfer((arg >> 8) & 0xFF);
  spi_transfer((arg >> 0) & 0xFF);
  spi_transfer(crc);

  /* Wait for response up to 10 bytes */
  uint8_t r = 0xFF;
  for (uint8_t i = 0; i < 10; i++) {
    r = spi_transfer(0xFF);
    if (r != 0xFF)
      break;
  }
  return r;
}

DSTATUS disk_initialize(void) {
  spi_init();

  /* 80 dummy clocks with CS high */
  CS_HIGH();
  for (uint8_t i = 0; i < 10; i++)
    spi_transfer(0xFF);

  /* CMD0 - reset card into SPI mode */
  uint8_t r;
  uint8_t attempts = 10;
  do {
    CS_LOW();
    r = send_cmd(0, 0);
    CS_HIGH();
    spi_transfer(0xFF);
    _delay_ms(10);
  } while (r != 0x01 && --attempts);

  if (r != 0x01)
    return STA_NOINIT; /* CMD0 failed */

  /* CMD8 - check voltage range, required for SDHC */
  CS_LOW();
  r = send_cmd(8, 0x1AA);
  /* read 4 byte R7 response */
  for (uint8_t i = 0; i < 4; i++)
    spi_transfer(0xFF);
  CS_HIGH();
  spi_transfer(0xFF);

  /* ACMD41 - wait for card to finish init */
  uint16_t timeout = 2000;
  do {
    /* CMD55 - app command prefix */
    CS_LOW();
    send_cmd(55, 0);
    CS_HIGH();
    spi_transfer(0xFF);

    /* CMD41 - send host capacity support */
    CS_LOW();
    r = send_cmd(41, (1UL << 30));
    CS_HIGH();
    spi_transfer(0xFF);

    _delay_ms(1);
  } while (r != 0x00 && --timeout);

  if (timeout == 0)
    return STA_NOINIT; /* ACMD41 timed out */

  spi_fast();
  return 0;
}

DRESULT disk_readp(BYTE *buf, DWORD sector, UINT offset, UINT count) {
  CS_LOW();
  if (send_cmd(17, sector) != 0x00) {
    CS_HIGH();
    spi_transfer(0xFF);
    return RES_ERROR;
  }

  /* Wait for data token 0xFE */
  uint16_t timeout = 10000;
  uint8_t token;
  do {
    token = spi_transfer(0xFF);
    if (--timeout == 0) {
      CS_HIGH();
      spi_transfer(0xFF);
      return RES_ERROR;
    }
  } while (token != 0xFE);

  for (uint16_t i = 0; i < 512; i++) {
    uint8_t b = spi_transfer(0xFF);
    if (i >= offset && i < (offset + count))
      *buf++ = b;
  }

  spi_transfer(0xFF); /* CRC high */
  spi_transfer(0xFF); /* CRC low  */
  CS_HIGH();
  spi_transfer(0xFF);
  return RES_OK;
}

DRESULT disk_writep(const BYTE *buf, DWORD sc) {
  if (sc == 0) {
    // /* Finalise - send stop token */
    // CS_LOW();
    // spi_transfer(0xFD);
    // spi_transfer(0xFF);
    // /* Wait while busy */
    // uint16_t timeout = 10000;
    // while (spi_transfer(0xFF) == 0x00) {
    //     if (--timeout == 0) { CS_HIGH(); return RES_ERROR; }
    // }
    // CS_HIGH();
    // spi_transfer(0xFF);

    CS_HIGH();
    spi_transfer(0xFF);
    return RES_OK;

  } else if (buf == 0) {
    /* Start sector write: sc = sector address */
    CS_LOW();
    if (send_cmd(24, sc) != 0x00) {
      CS_HIGH();
      spi_transfer(0xFF);
      return RES_ERROR;
    }
    spi_transfer(0xFF);
    spi_transfer(0xFE); /* data token */

  } else {
    /* Write 512 bytes */
    for (uint16_t i = 0; i < 512; i++) {
      spi_transfer(*buf++);
    }
    spi_transfer(0xFF); /* CRC */
    spi_transfer(0xFF);

    uint8_t r = spi_transfer(0xFF); /* data response */
    if ((r & 0x1F) != 0x05) {
      CS_HIGH();
      spi_transfer(0xFF);
      return RES_ERROR;
    }

    /* Wait while card is busy writing */
    uint16_t timeout = 10000;
    while (spi_transfer(0xFF) == 0x00) {
      if (--timeout == 0) {
        CS_HIGH();
        return RES_ERROR;
      }
    }
  }

  return RES_OK;
}
