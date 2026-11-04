#include "../host/axi_slave.h"
#include <cassert>
struct CPU {
    unsigned rst_n=1;
    unsigned axi_awready=0,axi_wready=0,axi_bvalid=0,axi_bresp=0,axi_arready=0,axi_rvalid=0,axi_rdata=0,axi_rresp=0;
    unsigned axi_bready=0,axi_rready=0,axi_awvalid=0,axi_awaddr=0,axi_wvalid=0,axi_wdata=0,axi_wstrb=0,axi_arvalid=0,axi_araddr=0;
};
int main() {
    AxiSlave s;CPU c;auto tick=[&]{s.drive(c);s.sample(c);};
    c.axi_wvalid=1;c.axi_wdata=73;c.axi_wstrb=15;tick();c.axi_wvalid=0;
    assert(s.duty==50 && s.acknowledgements==0);
    c.axi_awvalid=1;c.axi_awaddr=RV_DUTY;tick();c.axi_awvalid=0;
    assert(s.duty==73&&s.acknowledgements==1);
    for(int i=0;i<5;i++){tick();assert(c.axi_bvalid && !c.axi_awready && s.acknowledgements==1);}
    c.axi_bready=1;tick();c.axi_bready=0;
    s.push_key('A');s.push_key('B');
    c.axi_arvalid=1;c.axi_araddr=RV_EVENT;tick();c.axi_arvalid=0;
    for(int i=0;i<5;i++){tick();assert(c.axi_rvalid&&c.axi_rdata=='A'&&s.events.size()==1);}
    c.axi_rready=1;tick();assert(!s.rv);
    assert(s.read(RV_EVENT)=='B');assert(s.read(RV_EVENT)==0);
    for(unsigned i=0;i<70;i++)s.push_key(i+1);
    assert(s.events.size()==64&&s.dropped==6);
    s.write(RV_DUTY,200,15);assert(s.read(RV_DUTY)==100);
    s.write(RV_ENABLE,1,15);assert(s.read(RV_ENABLE)==1);
    s.timer_ms=0xfffffff0u;assert(s.read(RV_TIME)==0xfffffff0u);
    s.write(s.RAM_BASE,0x12345678,15);s.write(s.RAM_BASE+1,0x0000ab00,2);assert(s.read(s.RAM_BASE)==0x1234ab78);
    s.write(s.VRAM_BASE,0xff445566,15);assert(s.pixel_writes==1);
    bool caught=false;try{s.write(RV_ENABLE,0,1);}catch(const std::runtime_error&){caught=true;}assert(caught);
    std::cout<<"LVGL host AXI/MMIO tests PASS\n";
}
