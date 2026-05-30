#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <stdint.h>

void sd_logger_init(const char *header);
void sd_logger_write(const char *row);
void sd_logger_close(void);

#endif /* SD_LOGGER_H */
