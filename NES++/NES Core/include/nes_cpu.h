#ifndef NES_CPU_H
#define NES_CPU_H
#include <iostream>
#include <fstream>
#include "nes.h"

class NES;

class NES_CPU{
    private:
        uint8_t RAM[65536];
        uint16_t PC;
        uint16_t NMI;
        uint16_t t = 0;
        uint16_t XSCROLL;
        uint16_t YSCROLL;
        uint16_t VRAMADDR = 0x2000;
        uint8_t x;
        uint8_t A;
        uint8_t P = 0b10110100; // N V (1) B D I Z C
        bool N = 1, V = 0, U = 1, B = 0, D = 0, I = 1, Z = 0, C = 1;
        uint8_t SP = 0xFD;
        uint8_t X;
        uint8_t Y;
        uint32_t instruction;
        size_t rom_size;
        size_t max_size;
        static int cycles;
        void* instructionTable[255];
        NES* nes;
        uint8_t controllerShift = 0;
        int shiftCount = 0;
        int scanlines = 0;
        bool frame = 0;
        uint32_t ticksAtFrameEnd;
        bool frameEnd = false;

    public:
        void setA(uint8_t i ) {A = i;}
        uint8_t* getRAM() {return RAM;}
        void connectToNES(NES* ptrNES) {this->nes = ptrNES;}
        void init_cpu();
        void emulate_instruction(uint8_t* RAM);
        void adc(uint8_t operand);
        void sbc(uint8_t operand);
        void cmp(uint8_t main_operand, uint8_t operand);
        void update_status();
        void log_information();
        void cpuStep(int amount);
        void step(int amount);
        void handleAPURegisters(uint16_t address, uint8_t value);

        int getCycles() {return cycles;}
        int getScanlines() {return scanlines;}
        uint8_t getX() {return x;}
        uint16_t getXSCROLL() {return XSCROLL;}
        uint16_t getYSCROLL() {return YSCROLL;}
        uint16_t getT() {return t;}
        bool getFrameEnd() {return frameEnd;}


        void setPC(uint16_t value) {PC = value;}
        void setNMI(uint16_t value) {NMI = value;}
        void setShift(int n) {controllerShift = 0;}
        void setZ(bool z) {Z = z;}
        void update() {VRAMADDR = t;}
        void toggleFrameEnd() {frameEnd = false;}

        friend class NES_PPU;

};

#endif // NES_CPU_H
