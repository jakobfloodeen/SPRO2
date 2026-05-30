#include "sd_logger.h"
#include "diskio.h"
#include "pff.h"
#include <avr/io.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>

static FATFS fs;
static char filename[13];
static DWORD write_pos = 0;
static char sector[512];

/* ── LED ─────────────────────────────────────────────────── */
#define LED_DDR DDRB
#define LED_PORT PORTB
#define LED_PIN PB5

static void led_init(void) { LED_DDR |= (1 << LED_PIN); }
static void led_on(void) { LED_PORT |= (1 << LED_PIN); }
static void led_off(void) { LED_PORT &= ~(1 << LED_PIN); }

static void led_blink(uint8_t times) {
  for (uint8_t i = 0; i < times; i++) {
    led_on();
    _delay_ms(200);
    led_off();
    _delay_ms(200);
  }
  _delay_ms(500);
}

static void led_panic(void) {
  while (1) {
    led_on();
    _delay_ms(100);
    led_off();
    _delay_ms(100);
  }
}

/* ── sector write ────────────────────────────────────────── */
static void write_sector(const char *s) {
  memset(sector, 0, 512);
  uint16_t len = 0;
  while (s[len] && len < 510) {
    sector[len] = s[len];
    len++;
  }
  sector[len++] = '\r';
  sector[len++] = '\n';
  UINT bw;
  pf_write(sector, 512, &bw);
  write_pos += 512;
}

/* ── index tracking ──────────────────────────────────────── */
static uint8_t read_index(void) {
  if (pf_open("INDEX.TXT") != FR_OK)
    return 0;
  UINT br;
  pf_read(sector, 512, &br);
  uint8_t idx = sector[0] - '0';
  return (idx > 9) ? 0 : idx;
}

static void write_index(uint8_t idx) {
  if (pf_open("INDEX.TXT") != FR_OK)
    led_panic();
  memset(sector, 0, 512);
  sector[0] = '0' + (idx % 10);
  pf_lseek(0);
  UINT bw;
  pf_write(sector, 512, &bw);
  pf_write(0, 0, &bw);
}

/* ── public API ──────────────────────────────────────────── */
void sd_logger_init(const char *header) {
  led_init();
  led_off();

  led_blink(1);
  if (disk_initialize() != 0)
    led_panic();

  led_blink(2);
  if (pf_mount(&fs) != FR_OK)
    led_panic();

  led_blink(3);
  uint8_t idx = read_index();

  memcpy(filename, "DATA_0.CSV", 11);
  filename[5] = '0' + idx;

  write_index((idx + 1) % 10);

  if (pf_open(filename) != FR_OK)
    led_panic();

  led_blink(4);
  write_pos = 0;
  pf_lseek(0);
  write_sector(header);

  led_blink(5);
}

void sd_logger_write_row(const float *row) {
  memset(sector, 0, 512);
  uint16_t pos = 0;
  char tmp[14];

  for (uint8_t f = 0; f < LOG_FIELDS; f++) {
    dtostrf(row[f], 1, 3, tmp);
    uint8_t len = strlen(tmp);
    memcpy(&sector[pos], tmp, len);
    pos += len;
    if (f < LOG_FIELDS - 1)
      sector[pos++] = ',';
  }
  sector[pos++] = '\r';
  sector[pos++] = '\n';

  UINT bw;
  pf_write(sector, 512, &bw);
  write_pos += 512;

  led_blink(1);
}

void sd_logger_close(void) {
  UINT bw;
  pf_write(0, 0, &bw);
}
