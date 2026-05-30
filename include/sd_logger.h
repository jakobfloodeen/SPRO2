#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <stdint.h>

#define LOG_FIELDS 7
#define LOG_MEASUREMENTS 10

void sd_logger_init(const char *header);
void sd_logger_write_row(const float *row);
void sd_logger_close(void);

#endif
