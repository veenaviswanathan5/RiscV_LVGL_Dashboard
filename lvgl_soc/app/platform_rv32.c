#include "platform.h"
#include "protocol.h"
uint32_t platform_read(uint32_t a) {return *(volatile uint32_t *)(uintptr_t)a;}
void platform_write(uint32_t a,uint32_t v) {*(volatile uint32_t *)(uintptr_t)a=v;}
void platform_pixel(unsigned x,unsigned y,uint32_t v) {
    if(x>=320 || y>=240) platform_panic("pixel bounds",0);
    ((volatile uint32_t *)0x20000000u)[y*320+x]=v;
}
void platform_panic(const char *file,int line) {
    const char *s="LVGL assertion: ";while(*s) platform_write(RV_UART,(uint8_t)*s++);
    while(*file)platform_write(RV_UART,(uint8_t)*file++);
    platform_write(RV_UART,'\n');platform_write(RV_EXIT,(uint32_t)(line?line:99));
    for(;;) {}
}
