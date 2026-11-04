#include "../host/axi_slave.h"
extern "C" {
#include "dashboard.h"
#include "lvgl.h"
}
#include <cassert>
#include <cstdio>
#include <cstdlib>
static AxiSlave bus;
extern "C" uint32_t platform_read(uint32_t a) {return bus.read(a);}
extern "C" void platform_write(uint32_t a,uint32_t v) {bus.write(a,v,15);}
extern "C" void platform_pixel(unsigned x,unsigned y,uint32_t v) {assert(x<320&&y<240);bus.write(AxiSlave::VRAM_BASE+4*(y*320+x),v,15);}
extern "C" void platform_panic(const char *f,int n) {std::fprintf(stderr,"LVGL ASSERT %s:%d\n",f,n);std::abort();}
static void pump() {for(int i=0;i<4;i++){bus.timer_ms+=60;dashboard_step();}}
static void key(uint32_t k) {bus.push_key(k);pump();}
static void screenshot(const std::string& name) {
    pump();FILE *f=std::fopen(name.c_str(),"wb");assert(f);
    std::fprintf(f,"P6\n320 240\n255\n");
    for(auto p:bus.pixels) {unsigned char rgb[3]={(unsigned char)(p>>16),(unsigned char)(p>>8),(unsigned char)p};assert(std::fwrite(rgb,1,3,f)==3);}
    assert(std::fclose(f)==0);
}
int main(int argc,char **argv) {
    std::string dir=argc>1?argv[1]:".";
    dashboard_init();pump();
    assert(dashboard_selftest()==0);
    screenshot(dir+"/native-selftest.ppm");
    // Rapid programmatic tab changes must repaint the inactive tab captions.
    auto tv=lv_obj_get_child(lv_screen_active(),2);
    auto caption=lv_obj_get_child(lv_obj_get_child(lv_tabview_get_tab_bar(tv),1),0);
    lv_area_t area;lv_obj_get_coords(caption,&area);unsigned bright=0;
    for(int y=area.y1;y<=area.y2;y++)for(int x=area.x1;x<=area.x2;x++) {
        auto p=bus.pixels.at(y*320+x);
        if(((p>>16)&255)>200 && ((p>>8)&255)>200 && (p&255)>200)bright++;
    }
    assert(bright>20);
    // Real LVGL keypad events through the host MMIO queue.
    key(RV_K_F2);uint32_t before=bus.enable;
    key(13);assert(bus.enable!=before);
    key(RV_K_NEXT);uint32_t duty=bus.duty;
    key(RV_K_RIGHT);assert(bus.duty>duty);
    key(RV_K_F3);key('H');key('i');key(13);
    assert(bus.events.empty());
    dashboard_tab(0);screenshot(dir+"/monitor.ppm");
    dashboard_tab(1);screenshot(dir+"/control.ppm");
    dashboard_tab(2);screenshot(dir+"/logs.ppm");
    lv_mem_monitor_t mem;lv_mem_monitor(&mem);
    std::printf("LVGL native tests PASS. Free pool: %lu bytes; rendered words: %u\n",(unsigned long)mem.free_size,bus.pixel_writes);
    lv_deinit();return 0;
}
