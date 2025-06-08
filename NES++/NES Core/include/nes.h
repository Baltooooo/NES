#ifndef NES_H
#define NES_H
#include "nes_cpu.h"
#include "nes_ppu.h"
#include "nes_apu.h"
#include <iostream>

class NES_CPU;
class NES_PPU;
class NES_APU;

class NES
{
    private:
        uint8_t RAM[65536];
        NES_CPU* CPU;
        NES_PPU* PPU;
        NES_APU* APU;
        bool nametableMirroring;



    public:
        NES();
        void readConfig();
        void loadROM();
        void mapper(uint8_t mapperNumber);
        void write(uint16_t address, uint8_t value);
        void ppuWrite(uint16_t address, uint8_t value) {}
        uint8_t ppuRead(uint16_t address) {return RAM[address];}
        NES_CPU& getCPU() {return *CPU;}
        NES_PPU& getPPU() {return *PPU;}
        NES_APU& getAPU() {return *APU;}
        uint8_t* getRAM() {return RAM;}
        bool getMirrorType() {return nametableMirroring;}
        //Configuration
        string romPath;
        bool muted;
        bool fullscreen;
        bool logCPU;

    friend class NES_PPU;
};

#endif // NES_H
