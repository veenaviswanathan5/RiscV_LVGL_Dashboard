#include "dashboard.h"
#include "platform.h"
#include "protocol.h"
static void say(const char *s) {while(*s) platform_write(RV_UART,(uint8_t)*s++);}
int main(void) {
    say("Booting LVGL 9.2.2 on RV32IM...\n");
    uint32_t start=platform_read(RV_TIME);
    unsigned tries=0;
    while((uint32_t)(platform_read(RV_TIME)-start)<20u) {
        if(++tries==2000000u) {say("Timer probe failed\n");return 10;}
    }
    dashboard_init();
#ifdef DASH_SELFTEST
    int result=dashboard_selftest();
    say(result?"LVGL RV32 self-test FAIL\n":"LVGL RV32 self-test PASS\n");return result;
#else
    for(;;) dashboard_step();
#endif
}
