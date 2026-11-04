#ifndef DASH_PLATFORM_H
#define DASH_PLATFORM_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t platform_read(uint32_t addr);
void platform_write(uint32_t addr,uint32_t value);
void platform_pixel(unsigned x,unsigned y,uint32_t value);
void platform_panic(const char *file,int line);
#ifdef __cplusplus
}
#endif
#endif
