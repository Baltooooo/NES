#include <iostream>
#include <SDL2/SDL.h>
#include <nes_cpu.h>
#include <nes.h>

using namespace std;

NES NES;

int main(int argc, char* argv[]) {
    NES.loadROM();
    int counter = 0;
    bool frameStart = true;
    uint32_t start_time;
    uint32_t end_time;
    while(NES.getPPU().getState() == RUN){

        if(frameStart){
            start_time = SDL_GetTicks();
            frameStart = false;
        }

        NES.getPPU().inputHandler();
        for(int i = 0;i < 50; i++)
            NES.getCPU().emulate_instruction(NES.getRAM());

        //WIP Code
        if(NES.getCPU().getFrameEnd() == true){
            end_time = SDL_GetTicks();
            uint32_t elapsed_time = end_time - start_time;
            if(elapsed_time < 16.67)
                SDL_Delay(16.67-elapsed_time);
            NES.getCPU().toggleFrameEnd();
            frameStart = true;
        }

    }


   // for (int i = 0x2000, k = 0; i <= 0x23FF; i++, k++) {
   //  cout << hex << (int)NES.getPPU().getPPURAM()[i] << " ";
   // if (i % 32 == 0 && i != 0) {
   //     cout << endl;
   //     k = 0;
   // }
   //}
    NES.getPPU().logPPU();
    NES.getPPU().cleanup();


    return 0;
}
