#ifndef NES_PPU_H
#define NES_PPU_H
#include <iostream>
#include <SDL2/SDL.h>
#include "nes_cpu.h"


class NES;



enum State{
    RUN = 0,
    PAUSE,
    QUIT
};

class NES_PPU{
    private:
        NES* nes;
        SDL_Window *window;
        SDL_Renderer *renderer;
        SDL_Texture *texture;
        State emulatorState = RUN;
        int screenWidth = 32;
        int screenHeight = 30;
        int screenMult = 3;
        int hitScanline = -2;
        uint8_t PPU_RAM[16384];
        uint8_t OAM[256];
        uint8_t secondaryOAM[32];
        uint8_t PPUCTRL = 0;
        uint8_t PPUMASK = 0;
        uint8_t PPUSTATUS = 0x10;
        uint8_t OAMADDR = 0;
        uint8_t OAMDATA = 0;
        uint8_t PPUSCROLL = 0;
        uint8_t PPUADDR = 0;
        uint8_t PPUDATA = 0;
        uint8_t OAMDMA;
        uint8_t controllerState = 0;

        uint32_t systemPallete[64] = {
            0x666666FF, 0x001FB1FF, 0x2404C6FF, 0x5100B1FF,
            0x720075FF, 0x7F0024FF, 0x720B00FF, 0x512800FF,
            0x244300FF, 0x005600FF, 0x005B00FF, 0x005224FF,
            0x003C75FF, 0x000000FF, 0x000000FF, 0x000000FF,
            0xAAAAAAFF, 0x0D56FDFF, 0x4A30FDFF, 0x8913FDFF,
            0xBB08D4FF, 0xD01268FF, 0xC52E00FF, 0x9C5300FF,
            0x5F7A00FF, 0x209700FF, 0x00A200FF, 0x009841FF,
            0x007CB3FF, 0x000000FF, 0x000000FF, 0x000000FF,
            0xFDFDFDFF, 0x52ADFDFF, 0x8F84FDFF, 0xD164FDFF,
            0xFD56FDFF, 0xFD5CCDFF, 0xBCC500FF, 0xF89D00FF,
            0xBCC500FF, 0x79E500FF, 0x42F411FF, 0x26ED7DFF,
            0x2CD3F4FF, 0x4D4D4DFF, 0x000000FF, 0x000000FF,
            0xFDFDFDFF, 0xB5DFFDFF, 0xCCCFFDFF, 0xE7C1FDFF,
            0xFDBBFDFF, 0xFDBCF2FF, 0xFDC4C1FF, 0xFDD399FF,
            0xE7E480FF, 0xCCF280FF, 0xB5F999FF, 0xA8F8C1FF,
            0xA8EEF2FF, 0xB7B7B7FF, 0x000000FF, 0x000000FF

        };
        uint8_t framePalletes[32*30];



    public:
        NES_PPU();
        void logPPU();
        void connectToNES(NES* nes){this->nes = nes;}
        uint8_t read(uint16_t address) {return PPU_RAM[address];}
        uint8_t* getPPURAM() {return PPU_RAM;}
        uint8_t getCTRL() {return PPUCTRL;}
        uint8_t getMASK() {return PPUMASK;}
        uint8_t getSTATUS() {return PPUSTATUS;}
        uint8_t getOAMADDR() {return OAMADDR;}
        uint8_t getOAMDATA() {return OAMDATA;}
        uint8_t getSCROLL() {return PPUSCROLL;}
        uint8_t getADDR() {return PPUADDR;}
        uint8_t getDATA() {return PPUDATA;}
        uint8_t getControllerState() {return controllerState;}
        int getHit() {return hitScanline;}
        bool isSprite0(uint8_t spriteY, uint16_t spriteTile, uint16_t spriteAttribute, uint8_t spriteX);
        State getState() {return emulatorState;}


        void cleanup();
        void inputHandler();
        void updateScreen();
        void step(int amount);
        void selectFullscreen();
        void present();
        void renderPixel(int scanlines, int dot, int pixel, int pallete);
        void renderPixels(int scanlines, int prev, int delta);
        void renderLoopy(int scanlines, int dot, uint8_t MSB, uint8_t LSB, uint16_t pallete);
        void renderSprite(int scanlines, int spriteX, uint8_t MSB, uint8_t LSB, uint16_t pallete, bool isSprite0);
        void clearScreen() {SDL_RenderClear(renderer);}
        void write(uint16_t address, uint8_t value) {PPU_RAM[address] = value;}
        void writeOAM(uint16_t address, uint8_t value) {OAM[address] = value;}
        uint8_t readOAM(uint16_t address) {return OAM[address];}
        void writeSecondaryOAM(uint16_t address, uint8_t value) {secondaryOAM[address] = value;}
        uint8_t readSecondaryOAM(uint16_t address) {return secondaryOAM[address];}
        void setCTRL(uint8_t value) {PPUCTRL = value;}
        void setMASK(uint8_t value) {PPUMASK = value;}
        void setSTATUS(uint8_t value) {PPUSTATUS = value;}
        void setOAMADDR(uint8_t value) {OAMADDR = value;}
        void setOAMDATA(uint8_t value) {OAMDATA = value;}
        void setSCROLL(uint8_t value) {PPUSCROLL = value;}
        void setADDR(uint8_t value) {PPUADDR = value;}
        void setDATA(uint8_t value) {PPUDATA = value;}
        void setHit(int value) {hitScanline = value;}


        friend class NES_CPU;

};

#endif // NES_PPU_H
