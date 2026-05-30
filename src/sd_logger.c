#include "sd_logger.h"
#include "diskio.h"
#include "pff.h"
#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <util/delay.h>

static FATFS fs;
static char filename[15];
static DWORD write_pos = 0;

/* ── LED helpers ─────────────────────────────────────────── */
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

/* ── internal ────────────────────────────────────────────── */
static void write_sector(const char *s) {
  static char buf[512];
  memset(buf, 0, 512);

  uint16_t len = 0;
  while (s[len] && len < 510) {
    buf[len] = s[len];
    len++;
  }
  buf[len++] = '\r';
  buf[len++] = '\n';

  UINT bw;
  pf_write(buf, 512, &bw);
  write_pos += 512;
}

/* Read current index from INDEX.TXT, returns 0-9 */
static uint8_t read_index(void) {
  if (pf_open("INDEX.TXT") != FR_OK)
    return 0;

  static char buf[512];
  UINT br;
  if (pf_read(buf, 512, &br) != FR_OK)
    return 0;
  if (br == 0)
    return 0;

  uint8_t idx = buf[0] - '0';
  if (idx > 9)
    idx = 0;
  return idx;
}

/* Write next index to INDEX.TXT */
static void write_index(uint8_t idx) {
  if (pf_open("INDEX.TXT") != FR_OK)
    led_panic();

  static char buf[512];
  memset(buf, 0, 512);
  buf[0] = '0' + (idx % 10);

  pf_lseek(0);
  UINT bw;
  pf_write(buf, 512, &bw);
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

  /* Read current index, open that data file */
  uint8_t idx = read_index();
  sprintf(filename, "DATA_%d.CSV", idx);

  if (pf_open(filename) != FR_OK)
    led_panic();

  /* Write next index back to INDEX.TXT before opening data file */
  uint8_t next_idx = (idx + 1) % 10;
  write_index(next_idx);

  /* Re-open data file for writing */
  if (pf_open(filename) != FR_OK)
    led_panic();

  led_blink(4);
  write_pos = 0;
  pf_lseek(0);
  write_sector(header);

  led_blink(5);
}

void sd_logger_write(const char *row) {
  write_sector(row);
  led_blink(1);
}

void sd_logger_close(void) {
  UINT bw;
  pf_write(0, 0, &bw);
}
