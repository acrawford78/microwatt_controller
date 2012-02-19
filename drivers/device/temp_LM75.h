#ifndef LM75_TEMP_H
#include <stdint.h>
#define LM75_TEMP_H
#define LM75_TEMP_REG   0x00
#define LM75_CONFIG_REG 0x01

int16_t temp_LM75_shutdown(uint8_t shutdown, uint8_t saddr);
int16_t temp_LM75_read(float* temp_deg, uint8_t saddr);
int16_t temp_LM75_read_int(int16_t* temp, uint8_t saddr);
#endif
