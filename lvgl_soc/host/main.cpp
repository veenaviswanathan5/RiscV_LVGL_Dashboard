#include "VtopCPU.h"
#include "verilated.h"
#include "axi_slave.h"
#ifndef NO_SDL
#include <SDL2/SDL.h>
#endif
#include <string>
#include <memory>
#include <chrono>

int main(int argc,char** argv) {
    if(argc<2) { std::cerr<<"Usage: "<<argv[0]<<" firmware.bin [--headless] [--cycles N] [--progress] [--dump-ppm PATH] [--dump-ram PATH]\n"; return 2; }
    std::string dump_path, ram_path;
    bool headless=false,progress=false; uint64_t limit=100000000;
    try {
        for(int i=2;i<argc;i++) {
            std::string a=argv[i];
            if(a=="--headless") headless=true;
            else if(a=="--progress") progress=true;
            else if(a=="--dump-ppm" && i+1<argc) dump_path=argv[++i];
            else if(a=="--dump-ram" && i+1<argc) ram_path=argv[++i];
            else if(a=="--cycles" && i+1<argc) limit=std::stoull(argv[++i]);
            else throw std::runtime_error("Unknown/incomplete argument: "+a);
        }
        VerilatedContext context; context.commandArgs(argc,argv);
        auto cpu=std::make_unique<VtopCPU>(&context);
        AxiSlave bus; bus.load(argv[1]);
#ifndef NO_SDL
        SDL_Window* window=nullptr; SDL_Renderer* renderer=nullptr; SDL_Texture* texture=nullptr;
        if(!headless) {
            if(SDL_Init(SDL_INIT_VIDEO)!=0) throw std::runtime_error(SDL_GetError());
            window=SDL_CreateWindow("RV32 LVGL Dashboard",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,640,480,0);
            if(!window) throw std::runtime_error(SDL_GetError());
            renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
            if(!renderer) throw std::runtime_error(SDL_GetError());
            texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,320,240);
            if(!texture) throw std::runtime_error(SDL_GetError());
        }
        uint32_t last_frame=0;
#else
        if(!headless) throw std::runtime_error("Built without SDL: pass --headless");
#endif
        bool quit=false; uint64_t cycle=0;
        const auto timer_start=std::chrono::steady_clock::now();
        for(;cycle<limit && !context.gotFinish() && !bus.exited && !quit;cycle++) {
            if(progress && cycle && cycle%10000000==0)
                std::cerr<<"Progress: "<<std::dec<<cycle<<" cycles, fetch PC=0x"<<std::hex<<cpu->instrAddr<<std::dec<<"\n";
            cpu->clk=0; cpu->rst_n=(cycle>=8);
            bus.cycles=static_cast<uint32_t>(cycle);
            if(cycle%4096==0) bus.timer_ms=static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now()-timer_start).count());
            bus.drive(*cpu); cpu->eval();
            cpu->instrData=bus.fetch(cpu->instrAddr); cpu->eval();
            bus.sample(*cpu); // Snapshot before eval changes the master's state.
            cpu->clk=1; cpu->eval(); context.timeInc(1);
            cpu->clk=0; cpu->eval(); context.timeInc(1);
#ifndef NO_SDL
            if(!headless && cycle%4096==0) {
                SDL_Event event;
                while(SDL_PollEvent(&event)) {
                    if(event.type==SDL_QUIT) quit=true;
                    if(event.type!=SDL_KEYDOWN) continue;
                    auto key=event.key.keysym.sym;
                    auto mod=event.key.keysym.mod;
                    uint32_t code=0;
                    switch(key) {
                        case SDLK_TAB: code=(mod&KMOD_SHIFT)?RV_K_PREV:RV_K_NEXT;break;
                        case SDLK_LEFT:code=RV_K_LEFT;break;
                        case SDLK_RIGHT:code=RV_K_RIGHT;break;
                        case SDLK_UP:code=RV_K_UP;break;
                        case SDLK_DOWN:code=RV_K_DOWN;break;
                        case SDLK_F1:code=RV_K_F1;break;
                        case SDLK_F2:code=RV_K_F2;break;
                        case SDLK_F3:code=RV_K_F3;break;
                        case SDLK_RETURN:case SDLK_KP_ENTER:code=13;break;
                        case SDLK_BACKSPACE:code=8;break;
                        case SDLK_ESCAPE:code=27;break;
                        default:
                            if(key>=32 && key<=126 && !(mod&(KMOD_CTRL|KMOD_ALT))) {
                                code=static_cast<uint32_t>(key);
                                if(key>='a' && key<='z' && bool(mod&KMOD_SHIFT)!=bool(mod&KMOD_CAPS)) code-=32;
                                // Basic ASCII notes; shifted punctuation follows US layout.
                                if(mod&KMOD_SHIFT) {
                                    const char* plain="1234567890-=[]\\;',./`";
                                    const char* shifted="!@#$%^&*()_+{}|:\"<>?~";
                                    for(unsigned j=0;plain[j];j++) if(code==static_cast<uint32_t>(plain[j])) {
                                        code=static_cast<uint32_t>(shifted[j]);break;
                                    }
                                }
                            }
                    }
                    if(code) bus.push_key(code);
                }
                const Uint8* k=SDL_GetKeyboardState(nullptr);
                bus.keys=(k[SDL_SCANCODE_LEFT]?1:0)|(k[SDL_SCANCODE_RIGHT]?2:0)|
                         (k[SDL_SCANCODE_UP]?4:0)|(k[SDL_SCANCODE_DOWN]?8:0)|
                         (k[SDL_SCANCODE_SPACE]?16:0)|(k[SDL_SCANCODE_ESCAPE]?32:0);
                if(SDL_GetTicks()-last_frame>=16) {
                    SDL_UpdateTexture(texture,nullptr,bus.pixels.data(),320*4);
                    SDL_RenderClear(renderer); SDL_RenderCopy(renderer,texture,nullptr,nullptr); SDL_RenderPresent(renderer);
                    last_frame=SDL_GetTicks();
                }
            }
#endif
        }
        cpu->final();
        if(!ram_path.empty()) {
            std::ofstream out(ram_path,std::ios::binary);
            out.write(reinterpret_cast<const char*>(bus.ram.data()),bus.ram.size());
            if(!out) throw std::runtime_error("RAM dump failed");
        }
        if(!dump_path.empty()) {
            std::ofstream out(dump_path,std::ios::binary);
            if(!out) throw std::runtime_error("Cannot create framebuffer dump");
            out<<"P6\n320 240\n255\n";
            for(uint32_t p:bus.pixels) {
                char bytes[3]={char(p>>16),char(p>>8),char(p)};out.write(bytes,3);
            }
            if(!out) throw std::runtime_error("Framebuffer dump failed");
        }
#ifndef NO_SDL
        if(texture) SDL_DestroyTexture(texture);
        if(renderer) SDL_DestroyRenderer(renderer);
        if(window) SDL_DestroyWindow(window);
        SDL_Quit();
#endif
        std::cerr<<"Cycles: "<<cycle<<"\n";
        if(bus.exited) { std::cerr<<"Firmware exit: "<<bus.exit_code<<"\n"; return bus.exit_code?1:0; }
        if(quit) return 0;
        std::cerr<<"Stopped without firmware EXIT (cycle limit or $finish)\n"; return 2;
    } catch(const std::exception& e) { std::cerr<<"Emulator error: "<<e.what()<<"\n"; return 1; }
}
