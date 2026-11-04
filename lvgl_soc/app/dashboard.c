#include "lvgl.h"
#include "platform.h"
#include "protocol.h"
#include "dashboard.h"

static lv_obj_t *tabs,*pages[3],*uptime,*sweep,*stats,*enable_switch,*duty_slider;
static lv_obj_t *duty_text,*readback,*log_area,*note,*submit,*freeze_switch,*clear_button,*sweep_title;
static void monitor(lv_timer_t *timer);
static lv_group_t *group;
static lv_display_t *display;
static uint32_t draw_buffer[320*12] __attribute__((aligned(16)));
static char lines[6][48],log_text[300];
static unsigned line_count,tab_index;
static bool frozen,log_dirty=true,release_pending;
static uint32_t last_key,last_drops,flushes;

static uint32_t tick(void) {return platform_read(RV_TIME);}
static void uart(const char *s) {while(*s) platform_write(RV_UART,(uint8_t)*s++);}
static void log_add(const char *message) {
    if(line_count==6) {
        for(unsigned i=1;i<6;i++) lv_memcpy(lines[i-1],lines[i],sizeof lines[i]);
        line_count=5;
    }
    lv_snprintf(lines[line_count++],48,"%lu %s",(unsigned long)(tick()/1000u),message);
    log_dirty=true;
}
static void sync_log(void) {
    if(!log_dirty) return;
    unsigned pos=0;
    for(unsigned i=0;i<line_count;i++) {
        for(unsigned j=0;lines[i][j] && pos<sizeof(log_text)-2;j++) log_text[pos++]=lines[i][j];
        if(i+1<line_count) log_text[pos++]='\n';
    }
    log_text[pos]=0;
    lv_textarea_set_text(log_area,log_text);
    lv_textarea_set_cursor_pos(log_area,LV_TEXTAREA_CURSOR_LAST);
    log_dirty=false;
}
static void flush(lv_display_t *d,const lv_area_t *area,uint8_t *pixels) {
    const uint32_t *p=(const uint32_t *)pixels;
    for(int y=area->y1;y<=area->y2;y++) for(int x=area->x1;x<=area->x2;x++)
        platform_pixel((unsigned)x,(unsigned)y,(*p++)|0xff000000u);
    flushes++;
    lv_display_flush_ready(d);
}
static lv_obj_t *label(lv_obj_t *parent,int x,int y,const char *s) {
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_label_set_text(o,s);return o;
}
static lv_obj_t *button(lv_obj_t *parent,int x,int y,int w,const char *s) {
    lv_obj_t *o=lv_button_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,26);
    lv_obj_set_style_shadow_width(o,0,0);lv_obj_t *t=lv_label_create(o);lv_label_set_text(t,s);lv_obj_center(t);return o;
}
static void rebuild_group(void) {
    lv_group_remove_all_objs(group);
    lv_obj_t *bar=lv_tabview_get_tab_bar(tabs);
    for(unsigned i=0;i<3;i++) lv_group_add_obj(group,lv_obj_get_child(bar,(int)i));
    if(tab_index==1) {lv_group_add_obj(group,enable_switch);lv_group_add_obj(group,duty_slider);lv_group_focus_obj(enable_switch);}
    else if(tab_index==2) {
        lv_group_add_obj(group,note);lv_group_add_obj(group,submit);
        lv_group_add_obj(group,freeze_switch);lv_group_add_obj(group,clear_button);lv_group_focus_obj(note);
    } else lv_group_focus_obj(lv_obj_get_child(bar,0));
}
void dashboard_tab(unsigned tab) {
    if(tab>2) return;
    tab_index=tab;lv_tabview_set_active(tabs,tab,LV_ANIM_OFF);rebuild_group();
    if(tab==2) sync_log();
    if(tab==0) monitor(0);
    lv_obj_invalidate(lv_screen_active());
}
static void tab_changed(lv_event_t *e) {
    (void)e;tab_index=lv_tabview_get_tab_active(tabs);rebuild_group();if(tab_index==2) sync_log();
    lv_obj_invalidate(lv_screen_active());
}
static void update_readback(void) {
    uint32_t enabled=platform_read(RV_ENABLE),duty=platform_read(RV_DUTY),ack=platform_read(RV_ACK);
    lv_label_set_text_fmt(readback,"Readback: %s / %lu%%\nAcknowledged writes: %lu",enabled?"ON":"OFF",(unsigned long)duty,(unsigned long)ack);
}
static void control_changed(lv_event_t *e) {
    lv_obj_t *target=lv_event_get_target(e);char message[40];
    if(target==enable_switch) {
        uint32_t value=lv_obj_has_state(enable_switch,LV_STATE_CHECKED)?1u:0u;
        platform_write(RV_ENABLE,value);lv_snprintf(message,sizeof message,"Output %s",value?"enabled":"disabled");
    } else {
        uint32_t value=(uint32_t)lv_slider_get_value(duty_slider);
        platform_write(RV_DUTY,value);lv_label_set_text_fmt(duty_text,"%lu%%",(unsigned long)value);
        lv_snprintf(message,sizeof message,"Duty command %lu%%",(unsigned long)value);
    }
    log_add(message);update_readback();
}
static void note_submit(lv_event_t *e) {
    (void)e;const char *s=lv_textarea_get_text(note);
    if(*s) {log_add(s);lv_textarea_set_text(note,"");sync_log();}
}
static void freeze_changed(lv_event_t *e) {
    (void)e;frozen=lv_obj_has_state(freeze_switch,LV_STATE_CHECKED);
    log_add(frozen?"Monitor updates frozen":"Monitor updates resumed");sync_log();
}
static void clear_log(lv_event_t *e) {
    (void)e;line_count=0;log_add("Log cleared");sync_log();
}
static void read_keys(lv_indev_t *device,lv_indev_data_t *data) {
    (void)device;data->key=last_key;data->state=LV_INDEV_STATE_RELEASED;
    if(release_pending) {
        release_pending=false;data->continue_reading=platform_read(RV_EVENTS)!=0;return;
    }
    if(!platform_read(RV_EVENTS)) return;
    uint32_t raw=platform_read(RV_EVENT),key=raw;
    if(raw>=RV_K_F1 && raw<=RV_K_F3) {dashboard_tab(raw-RV_K_F1);data->continue_reading=true;return;}
    if(raw==27) {platform_write(RV_EXIT,0);return;}
    switch(raw) {
        case RV_K_NEXT:key=LV_KEY_NEXT;break;case RV_K_PREV:key=LV_KEY_PREV;break;
        case RV_K_LEFT:key=LV_KEY_LEFT;break;case RV_K_RIGHT:key=LV_KEY_RIGHT;break;
        case RV_K_UP:key=LV_KEY_UP;break;case RV_K_DOWN:key=LV_KEY_DOWN;break;
        case 13:key=LV_KEY_ENTER;break;case 8:key=LV_KEY_BACKSPACE;break;
        case 32:if(lv_group_get_focused(group)!=note) key=LV_KEY_ENTER;break;
        default:break;
    }
    last_key=key;data->key=key;data->state=LV_INDEV_STATE_PRESSED;
    release_pending=true;data->continue_reading=true;
}
static void monitor(lv_timer_t *timer) {
    (void)timer;uint32_t ms=tick(),drops=platform_read(RV_DROPS);
    if(drops!=last_drops) {log_add("Keyboard queue overflow");last_drops=drops;}
    if(tab_index==0) lv_label_set_text(sweep_title,frozen?"Updates paused in Logs":"10-second timer sweep");
    if(tab_index==0 && !frozen) {
        lv_label_set_text_fmt(uptime,"Uptime %lu s   |   Tick %lu ms",(unsigned long)(ms/1000u),(unsigned long)ms);
        lv_bar_set_value(sweep,(int32_t)((ms/100u)%100u),LV_ANIM_OFF);
        lv_label_set_text_fmt(stats,"AXI reads   %lu\nAXI writes  %lu\nVRAM words  %lu\nFlush areas %lu",
            (unsigned long)platform_read(RV_READS),(unsigned long)platform_read(RV_WRITES),
            (unsigned long)platform_read(RV_PIXELS),(unsigned long)flushes);
    }
    if(tab_index==2) sync_log();
}
void dashboard_init(void) {
    lv_init();lv_tick_set_cb(tick);
    display=lv_display_create(320,240);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_XRGB8888);
    lv_display_set_buffers(display,draw_buffer,0,sizeof draw_buffer,LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    lv_theme_t *theme=lv_theme_default_init(display,lv_color_hex(0x268ca8),lv_color_hex(0x63c9aa),true,&lv_font_montserrat_12);
    lv_display_set_theme(display,theme);
    lv_obj_t *screen=lv_screen_active();lv_obj_set_style_bg_color(screen,lv_color_hex(0x0a1320),0);
    label(screen,9,6,"RV32 / VIRTUAL SOC");
    lv_obj_t *badge=label(screen,232,6,"LVGL 9.2.2");lv_obj_set_style_text_color(badge,lv_color_hex(0x63c9aa),0);
    tabs=lv_tabview_create(screen);lv_obj_set_pos(tabs,0,26);lv_obj_set_size(tabs,320,194);
    lv_tabview_set_tab_bar_size(tabs,28);
    pages[0]=lv_tabview_add_tab(tabs,"Monitor");pages[1]=lv_tabview_add_tab(tabs,"Control");pages[2]=lv_tabview_add_tab(tabs,"Logs");
    for(int i=0;i<3;i++) {lv_obj_set_style_pad_all(pages[i],8,0);lv_obj_remove_flag(pages[i],LV_OBJ_FLAG_SCROLLABLE);}
    lv_obj_remove_flag(lv_tabview_get_content(tabs),LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *footer=label(screen,8,225,"F1-F3 tabs   Tab focus   Enter act   Esc exit");
    lv_obj_set_style_text_color(footer,lv_color_hex(0x8ba5b8),0);
    uptime=label(pages[0],0,0,"Timer starting...");
    sweep_title=label(pages[0],0,22,"10-second timer sweep");
    sweep=lv_bar_create(pages[0]);lv_obj_set_pos(sweep,0,43);lv_obj_set_size(sweep,288,10);lv_bar_set_range(sweep,0,99);
    stats=label(pages[0],0,66,"Bus counters loading...");
    label(pages[1],0,2,"Virtual output enable");
    enable_switch=lv_switch_create(pages[1]);lv_obj_set_pos(enable_switch,245,0);lv_obj_set_size(enable_switch,42,22);
    if(platform_read(RV_ENABLE))lv_obj_add_state(enable_switch,LV_STATE_CHECKED);
    lv_obj_set_style_anim_duration(enable_switch,0,0);
    label(pages[1],0,34,"Duty command");duty_text=label(pages[1],246,34,"50%");
    duty_slider=lv_slider_create(pages[1]);lv_obj_set_pos(duty_slider,8,66);lv_obj_set_size(duty_slider,270,10);
    lv_slider_set_range(duty_slider,0,100);lv_slider_set_value(duty_slider,(int32_t)platform_read(RV_DUTY),LV_ANIM_OFF);
    readback=label(pages[1],0,90,"");update_readback();
    lv_obj_t *hint=label(pages[1],0,132,"Host MMIO latch / no physical actuator");lv_obj_set_style_text_color(hint,lv_color_hex(0x8ba5b8),0);
    log_area=lv_textarea_create(pages[2]);lv_obj_set_pos(log_area,0,0);lv_obj_set_size(log_area,288,70);
    /* log_text is bounded; a widget max length forces expensive per-character insertion. */
    lv_textarea_set_max_length(log_area,0);lv_textarea_set_text(log_area,"");
    lv_obj_set_style_pad_all(log_area,4,0);
    lv_obj_set_style_border_width(log_area,1,0);
    lv_obj_set_style_text_line_space(log_area,0,0);lv_obj_set_style_bg_opa(log_area,LV_OPA_TRANSP,LV_PART_CURSOR);
    lv_obj_remove_flag(log_area,LV_OBJ_FLAG_CLICK_FOCUSABLE);
    note=lv_textarea_create(pages[2]);lv_obj_set_pos(note,0,78);lv_obj_set_size(note,200,29);
    lv_textarea_set_one_line(note,true);lv_textarea_set_max_length(note,32);lv_textarea_set_placeholder_text(note,"Type a note...");
    lv_obj_set_style_pad_all(note,4,0);
    submit=button(pages[2],208,79,80,"Add");
    freeze_switch=lv_switch_create(pages[2]);lv_obj_set_pos(freeze_switch,0,119);lv_obj_set_size(freeze_switch,32,18);
    lv_obj_set_style_anim_duration(freeze_switch,0,0);label(pages[2],39,120,"Freeze monitor");
    clear_button=button(pages[2],208,115,80,"Clear");
    lv_obj_add_event_cb(enable_switch,control_changed,LV_EVENT_VALUE_CHANGED,0);
    lv_obj_add_event_cb(duty_slider,control_changed,LV_EVENT_VALUE_CHANGED,0);
    lv_obj_add_event_cb(note,note_submit,LV_EVENT_READY,0);
    lv_obj_add_event_cb(submit,note_submit,LV_EVENT_CLICKED,0);
    lv_obj_add_event_cb(freeze_switch,freeze_changed,LV_EVENT_VALUE_CHANGED,0);
    lv_obj_add_event_cb(clear_button,clear_log,LV_EVENT_CLICKED,0);
    group=lv_group_create();lv_group_set_wrap(group,true);
    lv_indev_t *keyboard=lv_indev_create();lv_indev_set_type(keyboard,LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keyboard,read_keys);lv_indev_set_group(keyboard,group);
    lv_obj_add_event_cb(tabs,tab_changed,LV_EVENT_VALUE_CHANGED,0);
    dashboard_tab(0);log_add("Dashboard online");log_add("Timer and AXI link ready");
    lv_timer_create(monitor,500,0);monitor(0);
    uart("LVGL dashboard ready. F1/F2/F3 tabs; Tab focus; Enter activate; Esc exit.\n");
}
void dashboard_step(void) {lv_timer_handler();}
static bool contains(const char *text,const char *needle) {
    for(;*text;text++) {unsigned i=0;while(needle[i] && text[i]==needle[i])i++;if(!needle[i])return true;}
    return false;
}
int dashboard_selftest(void) {
    uart("Self-test: control write/readback\n");
    uint32_t ack=platform_read(RV_ACK);
    dashboard_tab(1);
    lv_obj_add_state(enable_switch,LV_STATE_CHECKED);lv_obj_send_event(enable_switch,LV_EVENT_VALUE_CHANGED,0);
    lv_slider_set_value(duty_slider,73,LV_ANIM_OFF);lv_obj_send_event(duty_slider,LV_EVENT_VALUE_CHANGED,0);
    if(platform_read(RV_ENABLE)!=1 || platform_read(RV_DUTY)!=73 || platform_read(RV_ACK)!=ack+2) return 21;
    uart("Self-test: text and bounded log\n");
    dashboard_tab(2);uart("Self-test: logs selected\n");
    lv_textarea_set_text(note,"RV32 note test");uart("Self-test: note set\n");
    lv_obj_send_event(note,LV_EVENT_READY,0);uart("Self-test: note submitted\n");sync_log();
    if(*lv_textarea_get_text(note) || !contains(lv_textarea_get_text(log_area),"RV32 note test")) return 22;
    uart("Self-test: bounded append\n");
    for(int i=0;i<20;i++) log_add("Bounded log check");
    sync_log();
    uart("Self-test: bounded sync done\n");
    if(line_count!=6 || lv_strlen(lv_textarea_get_text(log_area))>=300) return 23;
    lv_obj_add_state(freeze_switch,LV_STATE_CHECKED);lv_obj_send_event(freeze_switch,LV_EVENT_VALUE_CHANGED,0);
    if(!frozen) return 24;
    lv_obj_remove_state(freeze_switch,LV_STATE_CHECKED);lv_obj_send_event(freeze_switch,LV_EVENT_VALUE_CHANGED,0);
    if(frozen) return 25;
    for(unsigned i=0;i<3;i++) {uart("Self-test: render tab\n");dashboard_tab(i);lv_refr_now(display);}
    if(!flushes) return 26;
    lv_mem_monitor_t mem;lv_mem_monitor(&mem);
    if(mem.free_size<16384) return 27;
    return 0;
}
