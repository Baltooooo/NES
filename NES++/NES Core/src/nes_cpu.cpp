#include "nes_cpu.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <SDL2/SDL.h>

using namespace std;


uint16_t startPC;
uint16_t endPC;
uint16_t initialPC;
char assembly[15];
FILE *log = fopen("CPU-log.txt", "w");

bool caught = false;

int NES_CPU::cycles = 8;
int lineNumber = 1;
int cyclesToExecute = 8;
int ppuCycles = 3;
int totalCycles = 8;
int spriteIndex = 0;
int secondaryWrites = -1;
int spriteFetchCounter = 0;

bool w = true;
bool firstScroll = true;
bool firstBlank = true;
bool firstNMI = true;
bool first2007 = true;
bool allEvaluated = false;
bool inRange = false;

uint8_t buffer2007;

uint8_t nametableByte;
uint16_t tileIndex;
uint16_t attributeIndex;
uint8_t LSB;
uint8_t MSB;
uint8_t pallete;

uint16_t lowTileShifter;
uint16_t highTileShifter;
uint16_t lowAttributeShifter;
uint16_t highAttributeShifter;




struct SpriteRenderData{

    uint8_t spriteY;
    uint16_t spriteTile;
    uint16_t spriteAttribute;
    uint8_t spriteX;
    uint8_t spriteLSB;
    uint8_t spriteMSB;

}sr;

vector<SpriteRenderData> spriteRenderQueue;


uint16_t storedTileIndex[2];
uint16_t storedAttributeIndex[2];
uint8_t storedLSB[2];
uint8_t storedMSB[2];
uint8_t storedPallete[2];
uint16_t storedVRAM[2];

bool skip = false;


void NES_CPU::handleAPURegisters(uint16_t address, uint8_t value){


    if(nes->muted)
        return;

    switch(address){
        case 0x4008:
            nes->getAPU().triangle.lengthCounter.halt = (value >> 7) & 1;
            nes->getAPU().triangle.linearCounter.linearCounterLoad = value & 0b1111111;

        break;
        case 0x400A:
            nes->getAPU().triangle.lowRawT = value;
            nes->getAPU().triangle.timerLoad = (nes->getAPU().triangle.highRawT << 8) | nes->getAPU().triangle.lowRawT;


        break;
        case 0x400B:
            nes->getAPU().triangle.highRawT = value & 0b111;
            nes->getAPU().triangle.timerLoad = (nes->getAPU().triangle.highRawT << 8) | nes->getAPU().triangle.lowRawT;
            nes->getAPU().triangle.raw = nes->getAPU().triangle.timerLoad;

            nes->getAPU().triangle.lengthCounterLoadT = (value >> 3);
            nes->getAPU().triangle.lengthCounter.count = nes->getAPU().lengthLookupTable[nes->getAPU().triangle.lengthCounterLoadT];
            nes->getAPU().triangle.linearCounter.reloadFlag = true;
            nes->getAPU().triangle.sequenceIndex = 0;

        break;

        case 0x4015:
            nes->getAPU().p1.enable = value & 1;
            nes->getAPU().p2.enable = (value & 2) != 0;
            nes->getAPU().triangle.enable = (value & 8) != 0;
        break;
    }
}


void NES_CPU::step(int amount){

        for(int i = 0; i < amount; i++){
            ppuCycles++;

            while(ppuCycles >= 341){

                scanlines++;

                if(scanlines == 261){
                    RAM[0x2002] &= ~0x0b10000000;
                    nes->getPPU().setSTATUS(RAM[0x2002]);
                    scanlines = -1;
                    firstNMI = true;
                    firstBlank = true;
                    RAM[0x2002] &= ~0b010000000;
                    nes->getPPU().setSTATUS(RAM[0x2002]);
                    frameEnd = true;

                }
                if(scanlines == 239){
                    nes->getPPU().present();
                }
                if(scanlines == 241 && firstBlank){
                    RAM[0x2002] |= 0x80;
                    nes->getPPU().setSTATUS(RAM[0x2002]);

                    if(RAM[PC - 3] == 0xAD){
                        A = RAM[0x2002];
                        N = (A & 128) != 0;
                        Z = (A == 0);
                        update_status();
                    }
                    if((nes->getPPU().getCTRL() & 0x80) && nes->getPPU().getSTATUS() & 0x80 && firstNMI){
                        skip = true;

                    }

                    firstBlank = false;
                }
                ppuCycles -= 341;
                spriteIndex = -1;
                secondaryWrites = -1;
            }




            //Pre-render Pre-fetch escapades

            if(scanlines == -1 && ((ppuCycles >= 1 && ppuCycles <= 256) || (ppuCycles >= 321 && ppuCycles <= 336))  && (nes->getPPU().getMASK() & 0b1000 || nes->getPPU().getMASK() & 0b10000)){

                lowTileShifter <<= 1;
                highTileShifter <<= 1;

                lowAttributeShifter <<= 1;
                highAttributeShifter <<= 1;


                switch(ppuCycles % 8){
                case 1:
                        tileIndex = (0x2000 | (VRAMADDR) & 0x0FFF);
                    break;
                    case 3:
                        attributeIndex = 0x23C0 | (VRAMADDR & 0x0C00) | ((VRAMADDR >> 4) & 0x38) | ((VRAMADDR >> 2) & 0x07);
                    break;
                    case 5:
                        LSB = nes->getPPU().getPPURAM()[((nes->getPPU().getCTRL() & 0b00010000) ? 0x1000 : 0x0000) + nes->getPPU().getPPURAM()[tileIndex] * 16 + (VRAMADDR>>12)];
                    break;
                    case 7:
                        MSB = nes->getPPU().getPPURAM()[((nes->getPPU().getCTRL() & 0b00010000) ? 0x1000 : 0x0000) + nes->getPPU().getPPURAM()[tileIndex] * 16 + 8 + (VRAMADDR>>12)];
                        if ((VRAMADDR & 0x001F) == 31){ // if coarse X == 31
                        VRAMADDR &= ~0x001F;          // coarse X = 0
                        VRAMADDR ^= 0x0400;           // switch horizontal nametable
                    }else{
                            VRAMADDR += 1;
                    }
                    break;
                    case 0:

                        lowTileShifter |= LSB;
                        highTileShifter |= MSB;


                    break;
                }


            }

            //Background Rendering Pipeline

            if(((scanlines >= 0 && scanlines <= 239)) && ((ppuCycles >= 1 && ppuCycles <= 256) || (ppuCycles >= 321 && ppuCycles <= 336)) && (nes->getPPU().getMASK() & 0b1000 || nes->getPPU().getMASK() & 0b10000)){




                 if(ppuCycles >= 1 && ppuCycles <= 256){

                    int chosenBit = 0x8000 >> x;

                    int pixelLowBit = (lowTileShifter & chosenBit) > 0;
                    int pixelHighBit = (highTileShifter & chosenBit) > 0;

                    int palleteLowBit = (lowAttributeShifter & chosenBit) > 0;
                    int palleteHighBit = (highAttributeShifter & chosenBit) > 0;

                    int pixelBit = (pixelHighBit << 1) | pixelLowBit;
                    int palleteBit = (palleteHighBit << 1) | palleteLowBit;



                    nes->getPPU().renderPixel(scanlines, ppuCycles - 1, pixelBit, palleteBit);

                }

                lowTileShifter <<= 1;
                highTileShifter <<= 1;
                lowAttributeShifter <<= 1;
                highAttributeShifter <<= 1;




                switch((ppuCycles) % 8){
                    case 1:
                        tileIndex = (0x2000 | (VRAMADDR & 0x0FFF));
                    break;
                    case 3:
                        attributeIndex = nes->getPPU().getPPURAM()[0x23C0 | (VRAMADDR & 0x0C00) | ((VRAMADDR >> 4) & 0x38) | ((VRAMADDR >> 2) & 0x07)];
                        if(((VRAMADDR >> 5) & 0b11111) & 0x02) attributeIndex >>= 4;
                        if((VRAMADDR & 0b11111) & 0x02) attributeIndex >>= 2;
                        attributeIndex &= 0x03;
                    break;
                    case 5:
                        LSB = nes->getPPU().getPPURAM()[((nes->getPPU().getCTRL() & 0b00010000) ? 0x1000 : 0x0000) + nes->getPPU().getPPURAM()[tileIndex] * 16 + (VRAMADDR >> 12)];
                    break;
                    case 7:
                        MSB = nes->getPPU().getPPURAM()[((nes->getPPU().getCTRL() & 0b00010000) ? 0x1000 : 0x0000) + nes->getPPU().getPPURAM()[tileIndex] * 16 + 8 + (VRAMADDR >> 12)];
                        if ((VRAMADDR & 0x001F) == 31){ // if coarse X == 31
                        VRAMADDR &= ~0x001F;          // coarse X = 0
                        VRAMADDR ^= 0x0400;           // switch horizontal nametable
                    }else{
                            VRAMADDR += 1;
                    }
                    break;

                    case 0:

                        lowTileShifter |= LSB;
                        highTileShifter |= MSB;

                        lowAttributeShifter |= ((attributeIndex & 0b01) ? 0xFF : 0x00);
                        highAttributeShifter |= ((attributeIndex & 0b10) ? 0xFF : 0x00);

                        if(ppuCycles >= 1 && ppuCycles <= 256){

                                if(!spriteRenderQueue.empty()){
                                    for(int i = spriteRenderQueue.size() - 1; i >= 0; i--){
                                        if(scanlines >= spriteRenderQueue.at(i).spriteY){
                                            bool sprite0 = nes->getPPU().isSprite0(spriteRenderQueue.at(i).spriteY, spriteRenderQueue.at(i).spriteTile, spriteRenderQueue.at(i).spriteAttribute, spriteRenderQueue.at(i).spriteX);
                                            nes->getPPU().renderSprite(scanlines, spriteRenderQueue.at(i).spriteX + 8, spriteRenderQueue.at(i).spriteMSB, spriteRenderQueue.at(i).spriteLSB, spriteRenderQueue.at(i).spriteAttribute, sprite0);
                                        }
                                    }
                                }
                                if(ppuCycles == 256)
                                    spriteRenderQueue.clear();



                        }

                    break;

                }



            }


            //Sprite Evaluation

            if(scanlines >= -1 && scanlines <= 239 && (nes->getPPU().getMASK() & 0b1000 || nes->getPPU().getMASK() & 0b10000)){
                if(ppuCycles >= 0 && ppuCycles <= 64){
                    for(int i = 0; i < 32; i++){
                        nes->getPPU().writeSecondaryOAM(i, 0xFF);
                    }

                }


                if(ppuCycles >= 65 && ppuCycles <= 256 && spriteIndex <= 63){
                    uint8_t y;
                    if(ppuCycles % 2 != 0){
                        spriteIndex++;

                        y = nes->getPPU().readOAM(spriteIndex * 4);
                        if((scanlines >= y && y + 7 >= scanlines) && secondaryWrites <= 7 && y < 0xEF){
                            inRange = true;
                            secondaryWrites++;
                        }else{
                            inRange = false;
                        }
                    }else{
                        if(inRange && secondaryWrites <= 7){
                            for(int i = 0; i < 4; i++){
                                nes->getPPU().writeSecondaryOAM(secondaryWrites * 4 + i, nes->getPPU().readOAM(spriteIndex * 4 + i));
                            }
                        }
                    }
                }

                if(ppuCycles >= 257 && ppuCycles <= 320){

                    int spriteNum = (ppuCycles-257) / 8;
                    int spriteByte = (ppuCycles-257) % 8;

                    switch(spriteByte + 1){
                        case 1:
                            sr.spriteY = nes->getPPU().readSecondaryOAM(spriteNum * 4 + spriteByte);
                        break;
                        case 2:
                            sr.spriteTile = nes->getPPU().readSecondaryOAM(spriteNum * 4 + spriteByte);
                        break;
                        case 3:
                            sr.spriteAttribute = nes->getPPU().readSecondaryOAM(spriteNum * 4 + spriteByte);
                        break;
                        case 4:
                            sr.spriteX = nes->getPPU().readSecondaryOAM(spriteNum * 4 + spriteByte);
                            break;
                        case 5:
                            uint16_t spriteLocation = (nes->getPPU().getCTRL() & 0b00001000) ? 0x1000 : 0x0000;
                            uint16_t sprite = spriteLocation + (sr.spriteTile * 16);

                            bool verticalFlip = (sr.spriteAttribute >> 7);

                            if(verticalFlip){
                                sr.spriteLSB = nes->getPPU().getPPURAM()[sprite + 7 - ((scanlines) - (sr.spriteY))];
                                sr.spriteMSB = nes->getPPU().getPPURAM()[sprite + 8 + 7 - ((scanlines) - (sr.spriteY))];
                            }else{
                                sr.spriteLSB = nes->getPPU().getPPURAM()[sprite + (scanlines) - (sr.spriteY)];
                                sr.spriteMSB = nes->getPPU().getPPURAM()[sprite + 8 + (scanlines) - (sr.spriteY)];
                            }



                            if(sr.spriteY < 0xEF && spriteRenderQueue.size() <= 7){
                                spriteRenderQueue.push_back(sr);
                            }
                        break;
                    }
                }
            }

            //VRAM Address Frame Timed Operations

            if((ppuCycles == 256) && (scanlines < 240) && (nes->getPPU().getMASK() & 0b1000 || nes->getPPU().getMASK() & 0b10000)){
             if ((VRAMADDR & 0x7000) != 0x7000)
                VRAMADDR += 0x1000;
            else{
                VRAMADDR &= ~0x7000;
                int y = (VRAMADDR & 0x03E0) >> 5;
                if (y == 29){
                    y = 0;
                    VRAMADDR ^= 0x0800;
                }else if (y == 31)
                    y = 0;
                else
                    y += 1;
                VRAMADDR = (VRAMADDR & ~0x03E0) | (y << 5);
            }

            }
            if(ppuCycles == 257 && (scanlines < 240) && (nes->getPPU().getMASK() & 0b1000 || nes->getPPU().getMASK() & 0b10000)){
                VRAMADDR = (VRAMADDR & 0xFBE0) | (t & 0x041F);

            }
            if(scanlines == -1 && ppuCycles >= 280 && ppuCycles <= 304 && (nes->getPPU().getMASK() & 0b1000 || nes->getPPU().getMASK() & 0b10000)){
                VRAMADDR = (VRAMADDR & 0b000010000011111) | (t & 0b111101111100000);
            }
            if((ppuCycles == 328 || ppuCycles == 336 || ppuCycles <= 256) && (scanlines >= 0 && scanlines <= 239) && (nes->getPPU().getMASK() & 0b1000 || nes->getPPU().getMASK() & 0b10000)){

            }

    }
}


void NES_CPU::cpuStep(int amount){

    step(amount * 3);
    nes->getAPU().step(amount);
    totalCycles += amount;
}



void NES_CPU::log_information(){
    if(!nes->logCPU)
        return;

   fprintf(log, "PC: %04X Instruction: %02X %02X %02X, Assembly: %-15s      |A: %02X, X: %02X, Y: %02X, P: %02X, SP: %02X, CYC: %05d APU CYC: %05d | N:%d, V:%d, B:%d, D:%d, I:%d, Z:%d, C:%d|SCAN: %03d, DOT: %03d|PPUCTRL: %02X, PPUMASK: %02X, PPUSTATUS: %02X, OAMADDR: %02X, OAMDATA: %02X, PPUCSCROLL: %02X, PPUADDR: %02X, PPUDATA: %02X, VRAM ADDRESS: %04X| \n",
    PC - 1, (instruction >> 16) & 0xFF, (instruction >> 8) & 0xFF, instruction & 0xFF, assembly, A & 0xFF, X & 0xFF, Y & 0xFF, (P & ~0b00110000), SP & 0xFF, cycles, nes->getAPU().apuCycles, N, V, B, D, I, Z, C,
           scanlines, ppuCycles, nes->getPPU().getCTRL(), nes->getPPU().getMASK(), nes->getPPU().getSTATUS(), nes->getPPU().getOAMADDR(), nes->getPPU().getOAMDATA(), nes->getPPU().getSCROLL(), nes->getPPU().getADDR(), nes->getPPU().getDATA(), VRAMADDR);
}

void NES_CPU::update_status(){
    P = (N << 7) | (V << 6) | (1 << 5) | (B << 4) | (D << 3) | (I << 2) | (Z << 1) | C;
}

void NES_CPU::adc(uint8_t operand){
    int16_t result = (int16_t)A + (int16_t)operand + C;
    V = ((A > 127 && operand > 127 && result < 128) || (A < 128 && operand < 128 && result > 127));
    A = (result & 0xFF);
    N = (result & 128) != 0;
    Z = A == 0;
    C = result > 255;
    update_status();
}

void NES_CPU::sbc(uint8_t operand){
    int16_t result = (int16_t)A - (int16_t)operand - (1-C);
    V = ((A > 127 && operand < 128 && result < 128) || (A < 128 && operand > 127 && result >= 0));
    A = (result & 0xFF);
    N = (result & 128) != 0;
    Z = A == 0;
    C = result >= 0;
    update_status();
}

void NES_CPU::cmp(uint8_t main_operand, uint8_t operand){
    int16_t result = (int16_t)main_operand - (int16_t)operand;
    if(main_operand < operand){
        Z = 0;
        C = 0;
        N = (result & 128) != 0;
    }else if ((int16_t)main_operand == (int16_t)operand){
        Z = 1;
        C = 1;
        N = 0;
    }else{
        Z = 0;
        C = 1;
        N = (result & 128) != 0;
    }
    update_status();
}

void NES_CPU::emulate_instruction(uint8_t* RAM){

    uint8_t instruction_prefix = RAM[PC];
    instruction = instruction_prefix;
    initialPC = PC;
    cpuStep(cyclesToExecute);

    int cycles0 = cycles;
    switch(instruction_prefix){


        case 0x69:{
            //69 ADC #oper: Add immediate value with carry to accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ADC #%02X", (uint8_t)operand);
            instruction = (instruction << 8) | (uint8_t)operand;
            log_information();
            adc(operand);
            cycles += 2;

            PC++;
        }
        break;

        case 0x65:{
            //65 ADC zpg: Add zpg value with carry to accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ADC $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            adc(RAM[operand]);
            cycles += 3;

            PC++;
        }
        break;

        case 0x75:{
            //75 ADC zpg, X: Add zpg incremented by X value with carry to accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ADC $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            adc(RAM[operand]);
            cycles += 4;

            PC++;
        }
        break;

        case 0x6D:{
            //6D ADC abs: Add 16bit address value with carry to accumulator
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ADC $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            adc(RAM[target_address]);
            cycles += 4;

            PC++;
        }
        break;

        case 0x7D:{
            //7D ADC abs, X: Add 16bit address incremented by X value with carry to accumulator
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ADC $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + X) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += X;
            adc(RAM[target_address]);


            PC++;
        }
        break;

        case 0x79:{
            //79 ADC abs, Y: Add 16bit address incremented by Y value with carry to accumulator
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ADC $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + Y) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += Y;
            adc(RAM[target_address]);

            PC++;
        }
        break;

        case 0x61:{
            //61 ADC (oper, X): Add contents of word LL + X, LL + X + 1 to accumulator with carry
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            sprintf(assembly, "ADC ($%02X, X)", low - X);
            instruction = (instruction << 8) | (low - X);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();
            adc(RAM[effective_address]);

            cycles += 6;

            PC++;
        }
        break;

        case 0x71:{
            //71 ADC (oper), Y: Add contents of word (LL, LL + 1) + Y to accumulator with carry
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            sprintf(assembly, "ADC ($%02X), Y", low);
            instruction = (instruction << 8) | (low);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();

            if(((effective_address & 0xFF) + Y) > 255)
                cycles += 6;
            else
                cycles += 5;

            effective_address += Y;
            adc(RAM[effective_address]);

            PC++;
        }
        break;

        case 0x29:{
            //29 ANDA #oper: A &= immediate value
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ANDA #%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            A &= operand;

            N = (A & 128) != 0;
            Z = A == 0;
            update_status();

            cycles += 2;

            PC++;
        }
        break;

        case 0x25:{
            //25 ANDA zpg: A &= RAM[zpg]
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ANDA $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            A &= RAM[operand];

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 3;

            update_status();

            PC++;
        }
        break;

        case 0x35:{
            //35 ANDA zpg, X: A &= RAM[zpg + X]
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ANDA $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            A &= RAM[operand];

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 4;

            update_status();

            PC++;
        }
        break;

        case 0x2D:{
            //2D ANDA abs: A &= RAM[16bit]
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ANDA $%04X", target_address);
            log_information();
            A &= RAM[target_address];

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 4;

            update_status();

            PC++;
        }
        break;

        case 0x3D:{
            //3D ANDA abs, X: A &= RAM[16bit + X]
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ANDA $%04X, X", target_address);
            log_information();

            if(((target_address & 0xFF) + X) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += X;
            A &= RAM[target_address];

            N = (A & 128) != 0;
            Z = A == 0;



            update_status();

            PC++;
        }
        break;

        case 0x39:{
            //30 ANDA abs, Y: A &= RAM[16bit + Y]
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ANDA $%04X, Y", target_address);
            log_information();

            if(((target_address & 0xFF) + Y) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += Y;
            A &= RAM[target_address];

            N = (A & 128) != 0;
            Z = A == 0;



            update_status();

            PC++;
        }
        break;

        case 0x21:{
            //21 ANDA (oper, X): A &= (LL + X, LL + X + 1)
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            sprintf(assembly, "ANDA (%02X, X)", low - X);
            instruction = (instruction << 8) | (low - X);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();
            A &= RAM[effective_address];

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x31:{
            //31 ANDA (oper), Y: A &= (LL, LL + 1) + Y
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            sprintf(assembly, "ANDA (%02X, Y)", low);
            instruction = (instruction << 8) | low;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();

            if(((effective_address & 0xFF) + Y) > 255)
                cycles += 6;
            else
                cycles += 5;

            effective_address += Y;

            A &= RAM[effective_address];

            N = (A & 128) != 0;
            Z = A == 0;

            update_status();

            PC++;
        }
        break;

        case 0x0A:{
            //0A ASL A: Shift accumulator one bit to the left
            sprintf(assembly, "ASL A");
            log_information();
            C = (A >> 7) & 1;
            A <<= 1;
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 2;

            update_status();

            PC++;
        }
        break;

        case 0x06:{
            //06 ASL zpg: Shift zpg one bit to the left
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ASL $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            C = (RAM[operand] >> 7) & 1;
            RAM[operand] <<= 1;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 5;

            update_status();

            PC++;
        }
        break;

        case 0x16:{
            //16 ASL zpg, X: Shift zpg incremented by X one bit to the left
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ASL $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            C = (RAM[operand] >> 7) & 1;
            RAM[operand] <<= 1;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x0E:{
            //0E ASL abs: Shift 16 bit address value one bit to the left
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ASL $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            C = (RAM[target_address] >> 7) & 1;
            RAM[target_address] <<= 1;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x1E:{
            //1E ASL abs, X: Shift 16 bit address incremented by X value one bit to the left
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ASL $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;
            C = (RAM[target_address] >> 7) & 1;
            RAM[target_address] <<= 1;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0x90:{
            //F0 BCC rel: Branch if C == 0
            uint8_t operand = RAM[++PC];
            int8_t signed_operand = (int8_t)operand;
            sprintf(assembly, "BCC #%02X", operand);
            instruction = (instruction << 8) | (operand & 0xFF);
            log_information();
            uint16_t branchBase = PC + 1;
            if(C == 0){
                PC += (signed_operand + 1);

                if((branchBase & 0xFF00) != (PC & 0xFF00))
                    cycles +=4;
                else
                    cycles +=3;
            }else{
                cycles += 2;
                PC++;
            }
        }
        break;

        case 0xB0:{
            //F0 BCS rel: Branch if C == 1

            uint8_t operand = RAM[++PC];
            int8_t signed_operand = (int8_t)operand;
            sprintf(assembly, "BCS #%02X", operand);
            instruction = (instruction << 8) | (operand & 0xFF);
            log_information();
            uint16_t branchBase = PC + 1;
            if(C == 1){
                PC += (signed_operand + 1);


                if((branchBase & 0xFF00) != (PC & 0xFF00))
                    cycles +=4;
                else
                    cycles +=3;
            }else{
                PC++;
                cycles += 2;
            }
        }
        break;

        case 0xF0:{
            //F0 BEQ rel: Branch if Z == 1
            uint8_t operand = RAM[++PC];
            int8_t signed_operand = (int8_t)operand;
            sprintf(assembly, "BEQ #%02X", operand);
            instruction = (instruction << 8) | (operand & 0xFF);
            log_information();
            uint16_t branchBase = PC + 1;
            if(Z == 1){
                PC += (signed_operand + 1);
                if((branchBase & 0xFF00) != (PC & 0xFF00))
                    cycles +=4;
                else
                    cycles +=3;
            }else{
                PC++;
                cycles += 2;
            }
        }
        break;

        case 0x24:{
            //24 BIT, zpg: A AND ZPG, where M7 = N and M6 = V
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "BIT $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            uint8_t memory = RAM[operand];
            N = (memory & 128) != 0;
            V = (memory & 64) != 0;
            Z = (A & memory) == 0;

            cycles += 3;

            update_status();

            PC++;
        }
        break;

        case 0x2C:{
            //2C BIT, abs: A AND ABS, where M7 = N and M6 = V
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "BIT $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            uint8_t memory = RAM[target_address];
            N = (memory & 128) != 0;
            V = (memory & 64) != 0;
            Z = (A & memory) == 0;

            cycles += 4;

            update_status();

            PC++;
        }
        break;

        case 0x30:{
            //30 BMI rel: Branch if N == 1
            uint8_t operand = RAM[++PC];
            int8_t signed_operand = (int8_t)operand;
            sprintf(assembly, "BMI #%02X", operand);
            instruction = (instruction << 8) | (operand);
            log_information();
            uint16_t branchBase = PC + 1;
            if(N == 1){
                PC += (signed_operand + 1);
                if((branchBase & 0xFF00) != (PC & 0xFF00))
                    cycles +=4;
                else
                    cycles +=3;
            }else{
                PC++;
                cycles += 2;
            }
        }
        break;

        case 0xD0:{
            //D0 BNE rel: Branch if Z == 0
            uint8_t operand = RAM[++PC];
            int8_t signed_operand = (int8_t)operand;
            sprintf(assembly, "BNE #%02X", operand);
            instruction = (instruction << 8) | (operand);
            log_information();
            uint16_t branchBase = PC + 1;
            if(Z == 0){
                PC += (signed_operand + 1);
                if((branchBase & 0xFF00) != (PC & 0xFF00))
                    cycles +=4;
                else
                    cycles +=3;
            }else{
                PC++;
                cycles += 2;
            }
        }
        break;

        case 0x10:{
            //10 BPL rel: Branch if N == 0
            uint8_t operand = RAM[++PC];
            int8_t signed_operand = (int8_t)operand;
            sprintf(assembly, "BPL #%02X", operand);
            instruction = (instruction << 8) | (operand);
            log_information();
            uint16_t branchBase = PC + 1;
            if(N == 0){
                PC += (signed_operand + 1);
                if((branchBase & 0xFF00) != (PC & 0xFF00))
                    cycles +=4;
                else
                    cycles +=3;
            }else{
                PC++;
                cycles += 2;
            }
        }
        break;


        case 0x50:{
            //50 BVC rel: Branch if V == 0
            uint8_t operand = RAM[++PC];
            int8_t signed_operand = (int8_t)operand;
            sprintf(assembly, "BVC #%02X", operand);
            instruction = (instruction << 8) | (operand);
            log_information();
            uint16_t branchBase = PC + 1;
            if(V == 0){
                PC += (signed_operand + 1);
                if((branchBase & 0xFF00) != (PC & 0xFF00))
                    cycles +=4;
                else
                    cycles +=3;
            }else{
                PC++;
                cycles += 2;
            }
        }
        break;

        case 0x70:{
            //70 BVS rel: Branch if V == 1
            uint8_t operand = RAM[++PC];
            int8_t signed_operand = (int8_t)operand;
            sprintf(assembly, "BVS #%02X", operand);
            instruction = (instruction << 8) | (operand);
            log_information();
            uint16_t branchBase = PC + 1;
            if(V == 1){
                PC += (signed_operand + 1);
                if((branchBase & 0xFF00) != (PC & 0xFF00))
                    cycles +=4;
                else
                    cycles +=3;
            }else{
                PC++;
                cycles += 2;
            }
        }
        break;

        case 0x00:
            sprintf(assembly, "BRK");
            log_information();
            I = 1;
            P |= 0b00110000;
            RAM[0x100 + SP] = ((PC+2) >> 8) & 0xFF;
            SP--;
            RAM[0x100 + SP] = (PC+2) & 0xFF;
            SP--;
            RAM[0x100 + SP] = P;
            SP--;
            PC = (RAM[0xFFFF] << 8) | RAM[0xFFFE];

            cycles += 7;

            update_status();
        break;

        case 0x18:
            //18 CLC: Clear carry
            sprintf(assembly, "CLC");
            log_information();
            C = 0;
            cycles += 2;
            update_status();
            PC++;
            break;

        case 0xD8:
            //D8 CLD: Clear decimal addressing mode
            sprintf(assembly, "CLD");
            log_information();
            D = 0;
            cycles += 2;
            update_status();
            PC++;
            break;

        case 0x58:
            //58 CLI: Clear interrupt status
            sprintf(assembly, "CLI");
            log_information();
            I = 0;
            cycles += 2;
            update_status();
            PC++;
            break;

        case 0xB8:
            //B8 CLV: Clear overflow flag
            sprintf(assembly, "CLV");
            log_information();
            V = 0;
            cycles += 2;
            update_status();
            PC++;
            break;

        case 0xC9:{
            //C9 / CMP immediate: Compare immediate value with accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "CMP #%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            cmp(A, operand);

            cycles += 2;

            PC++;
        }
        break;

        case 0xC5:{
            //C5 CMP zpg: Compare zpg value with accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "CMP $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            cmp(A, RAM[operand]);

            cycles += 3;

            PC++;
        }
        break;

        case 0xD5:{
            //D5 CMP zpg, X: Compare zpg incremented by X value with accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "CMP $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            cmp(A, RAM[operand]);

            cycles += 4;

            PC++;
        }
        break;

        case 0xCD:{
            //CD CMP absolute: Compare value at 16 bit address with accumulator
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "CMP $%04X", (uint16_t)target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            cmp(A, RAM[target_address]);

            cycles += 4;

            PC++;
        }
        break;

        case 0xDD:{
            //DD CMP absolute, X: Compare value at 16 bit address which is incremented by X with accumulator
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "CMP $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + X) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += X;
            cmp(A, RAM[target_address]);

            PC++;
        }
        break;

        case 0xD9:{
            //D9 CMP absolute, Y: Compare value at 16 bit address which is incremented by Y with accumulator
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "CMP $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + Y) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += Y;
            cmp(A, RAM[target_address]);

            PC++;
        }
        break;

        case 0xC1:{
            //C1 CMP (oper, X): Compare value at word in (LL + X, LL + X + 1) with accumulator
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            sprintf(assembly, "CMP ($%02X, X)", low - X);
            instruction = (instruction << 8) | (low - X);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();
            cmp(A, RAM[effective_address]);

            cycles += 6;

            PC++;
        }
        break;

        case 0xD1:{
            //D1 CMP (oper), Y: Compare value at word in (LL, LL + 1) + Y with accumulator
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            sprintf(assembly, "CMP ($%02X), Y", low);
            instruction = (instruction << 8) | low;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();

            if(((effective_address & 0xFF) + Y) > 255)
                cycles += 6;
            else
                cycles += 5;

            effective_address += Y;
            cmp(A, RAM[effective_address]);

            PC++;
        }
        break;

        case 0xE0:{
            //E0 CPX #oper: Compare immediate value and index X
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "CPX #%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            cmp(X, operand);

            cycles += 2;

            PC++;
        }
        break;

        case 0xE4:{
            //E4 CPX zpg: Compare zpg value and index X
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "CPX $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            cmp(X, RAM[operand]);

            cycles += 3;

            PC++;
        }
        break;

        case 0xEC:{
            //EC CPX abs: Compare 16 bit address value and index X
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "CPX $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            cmp(X, RAM[target_address]);

            cycles += 4;

            PC++;
        }
        break;

        case 0xC0:{
            //C0 CPY #oper: Compare immediate value and index Y
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "CPY #%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            cmp(Y, operand);

            cycles += 2;

            PC++;
        }
        break;

        case 0xC4:{
            //C4 CPY zpg: Compare zpg value and index Y
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "CPY $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            cmp(Y, RAM[operand]);

            cycles += 3;

            PC++;
        }
        break;

        case 0xCC:{
            //CC CPY abs: Compare 16 bit address value and index Y
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "CPY $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            cmp(Y, RAM[target_address]);

            cycles += 4;

            PC++;
        }
        break;

        case 0xC7:{
            //C7 DCP zpg: Decrement memory then compare to accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "DCP $%02X", operand);
            instruction = (instruction << 8) | operand;
            RAM[operand]--;
            cmp(A, RAM[operand]);
            cycles += 5;
            PC++;
        }
        break;

        case 0xD7:{
            //D7 DCP zpg, X: Decrement and compare value in zpg incremented by X value with accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "DCP $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            RAM[operand]--;
            cmp(A, RAM[operand]);

            cycles += 6;

            PC++;
        }
        break;

        case 0xCF:{
            //CF DCP absolute: Decrement then compare value at 16 bit address with accumulator
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "DCP $%04X", (uint16_t)target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            RAM[target_address]--;
            cmp(A, RAM[target_address]);

            cycles += 6;

            PC++;
        }
        break;

        case 0xDF:{
            //DF DCP absolute, X: Decrement then compare value at 16 bit address which is incremented by X with accumulator
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "DCP $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            target_address += X;
            RAM[target_address]--;
            cmp(A, RAM[target_address]);

            cycles += 7;

            PC++;
        }
        break;

        case 0xDB:{
            //DB DCP absolute, Y: Decrement and compare value at 16 bit address which is incremented by Y with accumulator
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "DCP $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            target_address += Y;
            RAM[target_address]--;
            cmp(A, RAM[target_address]);

            cycles += 7;

            PC++;
        }
        break;

        case 0xC3:{
            //C3 DCP (oper, X): Decrement then compare value at word in (LL + X, LL + X + 1) with accumulator
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            sprintf(assembly, "DCP ($%02X, X)", low - X);
            instruction = (instruction << 8) | (low - X);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();
            RAM[effective_address]--;
            cmp(A, RAM[effective_address]);

            cycles += 8;

            PC++;
        }
        break;

        case 0xD3:{
            //D3 DCP (oper), Y: Decrement ompare value at word in (LL, LL + 1) + Y with accumulator
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            sprintf(assembly, "DCP ($%02X), Y", low);
            instruction = (instruction << 8) | low;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();

            effective_address += Y;
            RAM[effective_address]--;
            cmp(A, RAM[effective_address]);

            cycles += 8;

            PC++;
        }
        break;

        case 0xC6:{
            //C6 DEC zpg: Dencrement zpg by 1
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "DEC $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            RAM[operand]--;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 5;

            update_status();

            PC++;
        }
        break;

        case 0xD6:{
            //D6 DEC zpg, X: Decrement zpg incremented by X by 1
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "DEC $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            RAM[operand]--;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0xCE:{
            //CE DEC abs: Decrement 16bit address by 1
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "DEC $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            RAM[target_address]--;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0xDE:{
            //DE DEC abs, X: Decrement 16bit address incremented by X by 1
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "DEC $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;
            RAM[target_address]--;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0xCA:
            //CA DEX: Decrement the X register by 1
            sprintf(assembly, "DEX");
            log_information();
            X--;
            N = (X & 128) != 0;
            Z = (X == 0);
            update_status();
            cycles += 2;
            PC++;
            break;

        case 0x88:
            //88 DEY: Decrement the Y register by 1
            sprintf(assembly, "DEY");
            log_information();
            Y--;
            N = (Y & 128) != 0;
            Z = Y == 0;
            update_status();
            cycles += 2;
            PC++;
            break;

        case 0x49:{
            //49 EOR #oper: A ^= #oper
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "EOR #%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            A ^= operand;
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 2;

            update_status();

            PC++;
        }
        break;


        case 0x45:{
            //45 EOR zpg: A ^= RAM[zpg]
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "EOR $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            A ^= RAM[operand];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 3;

            update_status();

            PC++;
        }
        break;


        case 0x55:{
            //55 EOR zpg, X: A ^= RAM[zpg + X]
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "EOR $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            A ^= RAM[operand];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 4;

            update_status();

            PC++;
        }
        break;


        case 0x4D:{
            //4D EOR abs: A |= RAM[16bit]
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "EOR $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            A ^= RAM[target_address];

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 4;

            update_status();

            PC++;
        }
        break;

         case 0x5D:{
            //5D EOR abs, X: A |= RAM[16bit + X]
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "EOR $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + X) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += X;
            A ^= RAM[target_address];

            N = (A & 128) != 0;
            Z = A == 0;

            update_status();

            PC++;
        }
        break;

        case 0x59:{
            //59 EOR abs, Y: A |= RAM[16bit + Y]
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "EOR $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + Y) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += Y;
            A ^= RAM[target_address];

            N = (A & 128) != 0;
            Z = A == 0;

            update_status();

            PC++;
        }
        break;

        case 0x41:{
            //41 EOR (oper, X): A ^= (LL + X, LL + X + 1)
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            sprintf(assembly, "EOR (%02X, X)", low - X);
            instruction = (instruction << 8) | (low - X);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();
            A ^= RAM[effective_address];

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x51:{
            //51 EOR (oper), Y: A ^= (LL, LL + 1) + Y
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            sprintf(assembly, "EOR (%02X, Y)", low);
            instruction = (instruction << 8) | low;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();


            if(((effective_address & 0xFF) + Y) > 255)
                cycles += 6;
            else
                cycles += 5;

            effective_address += Y;

            if(effective_address > 0xFFFF)
                effective_address &= 0xFFFF;

            A ^= RAM[effective_address];

            N = (A & 128) != 0;
            Z = A == 0;

            update_status();

            PC++;
        }
        break;

        case 0xE6:{
            //E6 INC zpg: Increment zpg by 1
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "INC $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            RAM[operand]++;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 5;

            update_status();

            PC++;
        }
        break;

        case 0xF6:{
            //F6 INC zpg, X: Increment zpg incremented by X by 1
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "INC $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            RAM[operand]++;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0xEE:{
            //EE INC abs: Increment 16bit address by 1
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "INC $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            RAM[target_address]++;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0xFE:{
            //FE INC abs, X: Increment 16bit address incremented by X by 1
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "INC $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;
            RAM[target_address]++;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0xE8:
            //E8 INX: Increment X by 1
            sprintf(assembly, "INX");
            log_information();
            X++;
            N = (X & 128) != 0;
            Z = X == 0;

            cycles += 2;

            update_status();

            PC++;
            break;

        case 0xC8:
            //C8 INY: Increment Y by 1
            sprintf(assembly, "INY");
            log_information();
            Y++;
            N = (Y & 128) != 0;
            Z = Y == 0;

            cycles += 2;

            update_status();

            PC++;
            break;

        case 0xE7:{
            //E7 ISC zpg: Increment memory then SBC
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ISC $%02X", operand);
            instruction = (instruction << 8) | operand;
            RAM[operand]++;
            sbc(RAM[operand]);
            cycles += 5;
            PC++;
        }
        break;

        case 0xF7:{
            //F7 ISC zpg, X: Decrement and SBC value in zpg incremented by X value with accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ISC $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            RAM[operand]++;
            sbc(RAM[operand]);

            cycles += 6;

            PC++;
        }
        break;

        case 0xEF:{
            //EF ISC absolute: Increment then SBC value at 16 bit address
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ISC $%04X", (uint16_t)target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            RAM[target_address]++;
            sbc(RAM[target_address]);

            cycles += 6;

            PC++;
        }
        break;

        case 0xFF:{
            //FF ISC absolute, X: Increment then SBC value at 16 bit address which is incremented by X
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ISC $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            target_address += X;
            RAM[target_address]++;
            sbc(RAM[target_address]);

            cycles += 7;

            PC++;
        }
        break;

        case 0xFB:{
            //FB ISC absolute, Y: Increment and SBC value at 16 bit address which is incremented by Y
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ISC $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            target_address += Y;
            RAM[target_address]++;
            sbc(RAM[target_address]);

            cycles += 7;

            PC++;
        }
        break;

        case 0xE3:{
            //E3 ISC (oper, X): Increment then SBC value at word in (LL + X, LL + X + 1)
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            sprintf(assembly, "ISC ($%02X, X)", low - X);
            instruction = (instruction << 8) | (low - X);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();
            RAM[effective_address]++;
            sbc(RAM[effective_address]);

            cycles += 8;

            PC++;
        }
        break;

        case 0xF3:{
            //F3 ISC (oper), Y: Increment and SBC value at word in (LL, LL + 1) + Y
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            sprintf(assembly, "ISC ($%02X), Y", low);
            instruction = (instruction << 8) | low;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();

            effective_address += Y;
            RAM[effective_address]++;
            sbc(RAM[effective_address]);

            cycles += 8;

            PC++;
        }
        break;

        case 0x4C:{
            //4C JMP oper: Jump to new location
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            instruction = (instruction << 16) | target_address;
            sprintf(assembly, "JMP $%04X", target_address);
            log_information();
            cycles += 3;
            PC = target_address;
            if(scanlines == 240 && ((ppuCycles + (cycles - cycles0)*3) >= 341 && (ppuCycles + (cycles - cycles0)*3) - 341 != 1)){
                RAM[0x2002] |= 0x80;
                update_status();
                nes->getPPU().setSTATUS(RAM[0x2002]);
                firstBlank = false;
                if((nes->getPPU().getCTRL() & 0x80) && nes->getPPU().getSTATUS() & 0x80 && firstNMI){

                    I = 1;
                    B = 1;
                    update_status();
                    RAM[0x100 + SP] = (PC >> 8) & 0xFF;
                    SP--;
                    RAM[0x100 + SP] = PC & 0xFF;
                    SP--;
                    RAM[0x100 + SP] = (P);
                    SP--;
                    PC = NMI;
                    firstNMI = false;
                    cycles+=7;
                }

            }
        }
        break;

        case 0x6C:{
            //6C JMP (oper): Jump to contents of new location
            uint16_t target1 = RAM[++PC];
            target1 = (RAM[++PC] << 8) | target1;
            uint16_t target2 = target1 + 1;

            if((target1 & 0xFF) == 0xFF)
                target2 = (target1 & 0xFF00) | 0x00;

            uint16_t target_address = (RAM[target2] << 8) | RAM[target1];
            instruction = (instruction << 16) | target1;
            sprintf(assembly, "JMP ($%04X)", target1);
            log_information();
            cycles += 5;
            PC = target_address;
        }
        break;

        case 0x20:{
            //20 JSR: Jump to new address saving return address
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            instruction = (instruction << 16) | target_address;
            sprintf(assembly, "JSR $%04X", target_address);
            log_information();



            RAM[0x100 + SP] = (PC >> 8) & 0xFF;
            SP--;
            RAM[0x100 + SP] = (PC) & 0xFF;
            SP--;

            cycles += 6;

            PC = target_address;
        }
        break;

        case 0xA7:{
            //A7 LAX zpg: Load accumulator and X with value from zpg
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LAX $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();

            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 3;

            update_status();

            A = RAM[operand];
            X = RAM[operand];
            PC++;
        }
        break;

        case 0xB7:{
            //B7 LAX zpg, Y: Load accumulator and X with value from zpg with Y offset
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LAX $%02X, Y", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += Y;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 4;

            update_status();

            A = RAM[operand];
            X = RAM[operand];
            PC++;
        }
        break;

        case 0xAF:{
            //AF LAX abs: Load value from 16 bit address to accumulator and X
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "LAX $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            N = (RAM[target_address] & 128) != 0;
            Z = (RAM[target_address] == 0);


            cycles += 4;


            A = RAM[target_address];
            X = RAM[target_address];
            update_status();
            PC++;
         }
        break;

        case 0xBF:{
            //B3 LAX abs, Y: Load accumulator and X with value from 16 bit address and increment with Y
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "LAX $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + Y) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += Y;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;


            update_status();

            A = RAM[target_address];
            X = RAM[target_address];
            PC++;
        }
        break;

        case 0xA3:{
            //A3 LAX (oper), X: Load accumulator and X with memory from effective address LL + X, LL + X + 1
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            instruction = (instruction << 8) | (uint8_t)(low - X);
            sprintf(assembly, "LAX ($%02X), X", (uint8_t)(low - X));
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();


            N = (RAM[effective_address] & 128) != 0;
            Z = RAM[effective_address] == 0;

            cycles += 6;

            update_status();

            A = RAM[effective_address];
            X = RAM[effective_address];
            PC++;
        }
        break;

        case 0xB3:{
            //B3 LAX (oper), Y: Load accumulator and X with memory
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;

            instruction = (instruction << 8) | low;
            sprintf(assembly, "LAX (%02X), Y", low);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();

            if(((effective_address & 0xFF) + Y) > 255)
                cycles += 6;
            else
                cycles += 5;

            effective_address += Y;

            N = (RAM[effective_address] & 128) != 0;
            Z = RAM[effective_address] == 0;

            update_status();

            A = RAM[effective_address];
            X = RAM[effective_address];
            PC++;
        }
        break;

        case 0xA9:{
            //A9 LDA #oper: Load immediate value at accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LDA #%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();


            N = (operand & 128) != 0;
            Z = operand == 0;

            A = operand;

            cycles += 2;

            update_status();
            PC++;
        }
        break;

        case 0xA5:{
            //A5 LDA zpg: Load accumulator with value from zpg
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LDA $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();

            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 3;

            update_status();

            A = RAM[operand];
            PC++;
        }
        break;

        case 0xB5:{
            //B5 LDA zpg, x: Load accumulator with value from zpg with X offset
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LDA $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 4;

            update_status();

            A = RAM[operand];
            PC++;
        }
        break;

        case 0xAD:{
            //AD LDA abs: Load value from 16 bit address to accumulator
            uint8_t APUStatus;
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "LDA $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();


            if(target_address == 0x2007){
                 if(VRAMADDR < 0x3F00){
                    if(first2007){
                        first2007 = false;
                    }else{
                        RAM[target_address] = nes->getPPU().getPPURAM()[VRAMADDR];
                        VRAMADDR += (nes->getPPU().getCTRL() & 0x04) ? 32 : 1;
                    }
                }
            }


            if(target_address == 0x4015){
                APUStatus = 0;

                if(nes->getAPU().p1.lengthCounter.count > 0) APUStatus |= 0b00000001;
                if(nes->getAPU().p2.lengthCounter.count > 0) APUStatus |= 0b00000010;
                if(nes->getAPU().triangle.lengthCounter.count > 0) APUStatus |= 0b00000100;
                if(nes->getAPU().fc.interruptFlag) APUStatus |= 0b01000000;

                nes->getAPU().fc.interruptFlag = false;
            }

            if(target_address == 0x4016){
                if(shiftCount < 8){
                    RAM[target_address] = controllerShift & 0x1;
                    controllerShift >>= 1;
                    shiftCount++;
                }else{
                    RAM[target_address] = 1;
                }
            }

            if(target_address == 0x2002){
                if(scanlines != 241)
                    RAM[0x2002] &= ~0x80;
                else{
                    RAM[0x2002] = nes->getPPU().PPUSTATUS;
                }
                w = 0;
                nes->getPPU().setSTATUS(RAM[0x2002]);
            }

            if(target_address != 0x4015){
            N = (RAM[target_address] & 128) != 0;
            Z = (RAM[target_address] == 0);
            A = RAM[target_address];
            }else{
                N = (APUStatus & 128) != 0;
                Z = (APUStatus == 0);
                A = APUStatus;
            }

            cycles += 4;

            update_status();
            PC++;
         }
        break;

        case 0xBD:{
            //BD LDA abs, X: Load value from address incremented by X with carry into accumulator
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            instruction = (instruction << 16) | target_address;
            sprintf(assembly, "LDA $%04X, X", target_address);
            log_information();

            if(((target_address & 0xFF) + X) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address+=X;

            if(target_address == 0x4016){
                if(shiftCount < 8){
                    RAM[target_address] = controllerShift & 0x1;
                    controllerShift >>= 1;
                    shiftCount++;
                }else{
                    RAM[target_address] = 1;
                }
            }

            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            update_status();

            A = RAM[target_address];
            PC++;
        }
        break;

        case 0xB9:{
            //B9 LDA abs, Y: Load accumulator with value from 16 bit address and increment with Y
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "LDA $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + Y) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += Y;

            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;
            update_status();

            A = RAM[target_address];
            PC++;
        }
        break;

        case 0xA1:{
            //A1 LDA (oper), X: Load accumulator with memory from effective address LL + X, LL + X + 1
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            instruction = (instruction << 8) | (uint8_t)(low - X);
            sprintf(assembly, "LDA ($%02X), X", (uint8_t)(low - X));
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();


            N = (RAM[effective_address] & 128) != 0;
            Z = RAM[effective_address] == 0;

            cycles += 6;

            update_status();

            A = RAM[effective_address];
            PC++;
        }
        break;

        case 0xB1:{
            //B1 LDA (oper), Y: Load accumulator with memory
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;



            instruction = (instruction << 8) | low;
            sprintf(assembly, "LDA (%02X), Y", low);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();

            if(((effective_address & 0xFF) + Y) > 255)
                cycles += 6;
            else
                cycles += 5;

            effective_address += Y;

            N = (RAM[effective_address] & 128) != 0;
            Z = RAM[effective_address] == 0;

            update_status();

            A = RAM[effective_address];
            PC++;
        }
        break;

        case 0xA2:{
            //A2 LDX #oper: Load immediate value into X
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LDX #%02X", operand);
            instruction = (instruction << 8) | (uint8_t)operand;
            log_information();
            N = (operand & 128) != 0;
            Z = (operand) == 0;

            cycles += 2;

            update_status();

            X = operand;

            PC++;
        }
        break;

        case 0xA6:{
            //A6 LDX zpg: Load zpg value into X
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LDX $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 3;

            update_status();

            X = RAM[operand];
            PC++;
        }
        break;

        case 0xB6:{
            //B6 LDX zpg, Y: Load zpg incremented by Y into X
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LDX $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += Y;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 4;

            update_status();

            X = RAM[operand];
            PC++;
        }
        break;

        case 0xAE:{
            //AE LDX abs: Load 16 bit address value into X
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "LDX $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 4;

            update_status();

            X = RAM[target_address];

            if(target_address == 0x2002){
                RAM[0x2002] &= ~0x80;
                nes->getPPU().setSTATUS(RAM[0x2002]);
                X = nes->getPPU().getSTATUS();
            }
            PC++;
        }
        break;

        case 0xBE:{
            //BE LDX abs, Y: Load 16 bit address incremented by Y into X
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "LDX $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + Y) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address+=Y;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            update_status();

            X = RAM[target_address];
            PC++;
        }
        break;

        case 0xA0:{
            //A0 LDY #oper: Load Y with immediate value
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LDY #%02X", (uint8_t)operand);
            instruction = (instruction << 8) | (uint8_t)operand;
            log_information();
            N = (operand & 128) != 0;
            Z = operand == 0;

            cycles += 2;

            update_status();

            Y = operand;
            PC++;
        }
        break;

        case 0xA4:{
            //A4 LDY zpg: Load zpg into Y
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LDY $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 3;

            update_status();

            Y = RAM[operand];
            PC++;
        }
        break;

        case 0xB4:{
            //B4 LDY zpg, X: Load zpg incremented by X into Y
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LDY $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 4;

            update_status();

            Y = RAM[operand];
            PC++;
        }
        break;

        case 0xAC:{
            //AC LDY abs: Load 16 bit address into Y
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "LDY $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 4;

            update_status();

            Y = RAM[target_address];
             if(target_address == 0x2002){
                RAM[0x2002] &= ~0x80;
                nes->getPPU().setSTATUS(RAM[0x2002]);
                Y = nes->getPPU().getSTATUS();
            }
            PC++;
        }
        break;

        case 0xBC:{
            //BC LDY abs, X: Load 16 bit address incremented by X into Y
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "LDY $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + X) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += X;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            update_status();

            Y = RAM[target_address];
            PC++;

        }
        break;

        case 0x4A:{
            //4A LSR A: Shift accumulator b to the right by one bit
            sprintf(assembly, "LSR A");
            log_information();
            C = A & 1;
            A >>= 1;
            N = 0;
            Z = A == 0;

            cycles += 2;

            update_status();

            PC++;
        }
        break;

        case 0x46:{
            //46 LSR zpg: Shift value in zpg one bit to the right
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LSR $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            C = RAM[operand] & 1;
            RAM[operand] >>= 1;
            N = 0;
            Z = RAM[operand] == 0;

            cycles += 5;

            update_status();

            PC++;
        }
        break;

        case 0x56:{
            //56 LSR zpg, X: Shift value in zpg incremented by X one bit to the right
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "LSR $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            C = RAM[operand] & 1;
            RAM[operand] >>= 1;
            N = 0;
            Z = RAM[operand] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x4E:{
            //4E LSR abs: Shift value in 16 bit address one bit to the right
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "LSR $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            C = RAM[target_address] & 1;
            RAM[target_address] >>= 1;
            N = 0;
            Z = RAM[target_address] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x5E:{
            //5E LSR abs, X: Shift value in 16 bit address incremented by X one bit to the right
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "LSR $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;
            C = RAM[target_address] & 1;
            RAM[target_address] >>= 1;
            N = 0;
            Z = RAM[target_address] == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0x1A:
        case 0x3A:
        case 0x5A:
        case 0x7A:
        case 0xDA:
        case 0xFA:
        case 0xEA:
            //EA NOP: No operation;
            sprintf(assembly, "NOP");
            log_information();

            cycles += 2;

            PC++;
        break;


        case 0x80:
        case 0x82:
        case 0x89:
        case 0xC2:
        case 0xE2:{
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "NOP #%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();

            cycles += 2;
        }
        break;

        case 0x04:
        case 0x44:
        case 0x64:{
            //04 NOP zpg: No operation;
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "NOP $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();

            cycles += 3;

            PC++;
        }
        break;

        case 0x14:
        case 0x34:
        case 0x54:
        case 0x74:
        case 0xD4:
        case 0xF4:{
            //NOP zpg, X: No operation
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "NOP $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            operand += X;
            log_information();

            cycles += 4;
            PC++;
        }
        break;

        case 0x0C:{
            //NOP abs: No operation
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "NOP $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            cycles += 4;
            PC++;
        }
        break;

        case 0x1C:
        case 0x3C:
        case 0x5C:
        case 0x7C:
        case 0xDC:
        case 0xFC:{
            //NOP abs, X
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "NOP $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;

            if(((target_address & 0xFF) + X) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += X;
            log_information();

            PC++;
        }
        break;

        case 0x09:{
            //09 ORA #oper: A |= #oper
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ORA #%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            A |= operand;

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 2;

            update_status();

            PC++;
        }
        break;

        case 0x05:{
            //09 OR zpg: A |= RAM[zpg]
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ORA $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            A |= RAM[operand];

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 3;

            update_status();

            PC++;
        }
        break;

        case 0x15:{
            //19 ORA zpg, X: A |= RAM[zpg + X]
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ORA $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            A |= RAM[operand];

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 4;

            update_status();

            PC++;
        }
        break;

        case 0x0D:{
            //0D ORA abs: A |= RAM[16bit]
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ORA $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            A |= RAM[target_address];

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 4;

            update_status();

            PC++;
        }
        break;

        case 0x1D:{
            //1D ORA abs, X: A |= RAM[16bit + X]
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ORA $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + X) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += X;
            A |= RAM[target_address];

            N = (A & 128) != 0;
            Z = A == 0;

            update_status();

            PC++;
        }
        break;

        case 0x19:{
            //19 ORA abs, Y: A |= RAM[16bit + Y]
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ORA $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + Y) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += Y;

            A |= RAM[target_address];

            N = (A & 128) != 0;
            Z = A == 0;
            update_status();

            PC++;
        }
        break;

        case 0x01:{
            //01 ORA (oper, X): A |= (LL + X, LL + X + 1)
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            sprintf(assembly, "ORA (%02X, X)", low - X);
            instruction = (instruction << 8) | (low - X);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();
            A |= RAM[effective_address];

            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x11:{
            //11 ORA (oper), Y: A |= (LL, LL + 1) + Y
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            sprintf(assembly, "ORA (%02X, Y)", low);
            instruction = (instruction << 8) | low;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();

            if(((effective_address & 0xFF) + Y) > 255)
                cycles += 6;
            else
                cycles += 5;

            effective_address += Y;

            if(effective_address > 0xFFFF)
                effective_address &= 0xFFFF;

            A |= RAM[effective_address];

            N = (A & 128) != 0;
            Z = A == 0;

            update_status();

            PC++;
        }
        break;

        case 0x48:
            //48 PHA: Push Accumulator on Stack
            sprintf(assembly, "PHA");
            log_information();
            RAM[0x100 + SP] = A;
            SP--;

            cycles += 3;

            PC++;

        break;

        case 0x08:
            //08 PHP: Push Processor Status on Stack
            sprintf(assembly, "PHP");
            log_information();
            RAM[0x100 + SP] = P;
            RAM[0x100 + SP] |= 0b00110000;
            SP--;

            cycles += 3;

            PC++;
        break;

        case 0x68:
            //68 PLA: Pull accumulator from stack
            sprintf(assembly, "PLA");
            log_information();


            A = RAM[0x100 + ++SP];
            N = (A & 128) != 0;
            Z = A == 0;

            update_status();

            cycles += 4;

            PC++;
            break;

        case 0x28:{
            //28 PLP: Pull Processor status
            sprintf(assembly, "PLP");
            log_information();

            P = RAM[0x100 + (++SP)];
            N = (P & 0x80) != 0;
            V = (P & 0x40) != 0;
            B = (P & 0x10) != 0;
            D = (P & 0x08) != 0;
            I = (P & 0x04) != 0;
            Z = (P & 0x02) != 0;
            C = (P & 0x01) != 0;

            cycles += 4;

            PC++;
        }
        break;

        case 0x27:{
            //27 RLA zpg: Rotate zpg one bit the the left and then AND
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "RLA $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            uint8_t old_carry = C;
            C = (RAM[operand] >> 7) & 1;
            RAM[operand] <<= 1;
            RAM[operand] += old_carry;
            A &= RAM[operand];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 5;

            update_status();

            PC++;
        }
        break;

        case 0x37:{
            //37 RLA zpg, X: Rotate zpg incremented by X one bit the the left then AND
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "RLA $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            uint8_t old_carry = C;
            C = (RAM[operand] >> 7) & 1;
            RAM[operand] <<= 1;
            RAM[operand] += old_carry;
            A &= RAM[operand];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x2F:{
            //2F RLA abs: Rotate 16 bit address value one bit the the left then AND
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "RLA $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            uint8_t old_carry = C;
            C = (RAM[target_address] >> 7) & 1;
            RAM[target_address] <<= 1;
            RAM[target_address] += old_carry;
            A &= RAM[target_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x3F:{
            //3F RLA abs, X: Rotate 16 bit address incremented by X value one bit the the left then AND
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "RLA $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;
            uint8_t old_carry = C;
            C = (RAM[target_address] >> 7) & 1;
            RAM[target_address] <<= 1;
            RAM[target_address] += old_carry;
            A &= RAM[target_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0x3B:{
            //3B RLA abs, Y: Rotate 16 bit address incremented by Y value one bit the the left then AND
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "RLA $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += Y;
            uint8_t old_carry = C;
            C = (RAM[target_address] >> 7) & 1;
            RAM[target_address] <<= 1;
            RAM[target_address] += old_carry;
            A &= RAM[target_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0x23:{
            //23 RLA (oper, X): Rotate indirect address indexed by X value one bit the the left then AND
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            sprintf(assembly, "RLA ($%04X, X)", (uint8_t)(low - X));
            instruction = (instruction << 8) | (uint8_t)(low - X);
            log_information();
            uint8_t old_carry = C;
            C = (RAM[effective_address] >> 7) & 1;
            RAM[effective_address] <<= 1;
            RAM[effective_address] += old_carry;
            A &= RAM[effective_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 8;

            update_status();

            PC++;
        }
        break;

        case 0x33:{
            //33 RLA (oper), Y: Rotate indirect address indexed by Y value one bit the the left then AND
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            sprintf(assembly, "RLA ($%04X), Y", (uint8_t)(low - X));
            instruction = (instruction << 8) | (uint8_t)(low - X);
            log_information();
            effective_address += Y;
            uint8_t old_carry = C;
            C = (RAM[effective_address] >> 7) & 1;
            RAM[effective_address] <<= 1;
            RAM[effective_address] += old_carry;
            A &= RAM[effective_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 8;

            update_status();

            PC++;
        }
        break;

        case 0x2A:{
            //2A ROL accumulator: Rotate the accumulator one bit to the left
            sprintf(assembly, "ROL Acc");
            log_information();
            uint8_t old_carry = C;
            C = (A >> 7) & 1;
            A <<= 1;
            A += old_carry;
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 2;

            update_status();

            PC++;
        }
        break;

        case 0x26:{
            //26 ROL zpg: Rotate zpg one bit the the left
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ROL $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            uint8_t old_carry = C;
            C = (RAM[operand] >> 7) & 1;
            RAM[operand] <<= 1;
            RAM[operand] += old_carry;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 5;

            update_status();

            PC++;
        }
        break;

        case 0x36:{
            //36 ROL zpg, X: Rotate zpg incremented by X one bit the the left
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ROL $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            uint8_t old_carry = C;
            C = (RAM[operand] >> 7) & 1;
            RAM[operand] <<= 1;
            RAM[operand] += old_carry;
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x2E:{
            //2E ROL abs: Rotate 16 bit address value one bit the the left
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ROL $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            uint8_t old_carry = C;
            C = (RAM[target_address] >> 7) & 1;
            RAM[target_address] <<= 1;
            RAM[target_address] += old_carry;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x3E:{
            //3E ROL abs, X: Rotate 16 bit address incremented by X value one bit the the left
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ROL $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;
            uint8_t old_carry = C;
            C = (RAM[target_address] >> 7) & 1;
            RAM[target_address] <<= 1;
            RAM[target_address] += old_carry;
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0x6A:{
            //6A ROR accumulator: Rotate the accumulator one bit to the right
            sprintf(assembly, "ROR A");
            log_information();
            uint8_t old_carry = C;
            C = A & 1;
            A >>= 1;
            A += (old_carry << 7);
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 2;

            update_status();

            PC++;
        }
        break;


        case 0x66:{
            //66 ROR zpg: Rotate zpg one bit the the right
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ROR $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            uint8_t old_carry = C;
            C = RAM[operand] & 1;
            RAM[operand] >>= 1;
            RAM[operand] += (old_carry << 7);
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;


            cycles += 5;

            update_status();

            PC++;
        }
        break;

        case 0x76:{
            //76 ROR zpg, X: Rotate zpg incremented by X one bit the the right
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "ROR $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            uint8_t old_carry = C;
            C = RAM[operand] & 1;
            RAM[operand] >>= 1;
            RAM[operand] += (old_carry << 7);
            N = (RAM[operand] & 128) != 0;
            Z = RAM[operand] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x6E:{
            //6E ROR abs: Rotate 16 bit address value one bit the the right
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ROR $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            uint8_t old_carry = C;
            C = RAM[target_address] & 1;
            RAM[target_address] >>= 1;
            RAM[target_address] += (old_carry << 7);
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x7E:{
            //7E ROR abs, X: Rotate 16 bit address incremented by X value one bit the the right
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "ROR $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;
            uint8_t old_carry = C;
            C = RAM[target_address] & 1;
            RAM[target_address] >>= 1;
            RAM[target_address] += (old_carry << 7);
            N = (RAM[target_address] & 128) != 0;
            Z = RAM[target_address] == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0x67:{
            //67 RRA zpg: Rotate zpg one bit the the right then ADC
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "RRA $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            uint8_t old_carry = C;
            C = RAM[operand] & 1;
            RAM[operand] >>= 1;
            RAM[operand] += (old_carry << 7);
            adc(RAM[operand]);

            cycles += 5;

            PC++;
        }
        break;

        case 0x77:{
            //77 RRA zpg, X: Rotate zpg incremented by X one bit the the right then ADC
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "RRA $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            uint8_t old_carry = C;
            C = RAM[operand] & 1;
            RAM[operand] >>= 1;
            RAM[operand] += (old_carry << 7);
            adc(RAM[operand]);

            cycles += 6;

            PC++;
        }
        break;

        case 0x6F:{
            //6F RRA abs: Rotate 16 bit address value one bit the the right then ADC
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "RRA $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            uint8_t old_carry = C;
            C = RAM[target_address] & 1;
            RAM[target_address] >>= 1;
            RAM[target_address] += (old_carry << 7);
            adc(RAM[target_address]);

            cycles += 6;

            PC++;
        }
        break;

        case 0x7F:{
            //7F RRA abs, X: Rotate 16 bit address incremented by X value one bit the the right then ADC
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "RRA $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;
            uint8_t old_carry = C;
            C = RAM[target_address] & 1;
            RAM[target_address] >>= 1;
            RAM[target_address] += (old_carry << 7);
            adc(RAM[target_address]);

            cycles += 7;

            PC++;
        }
        break;

        case 0x7B:{
            //7B RRA abs, Y: Rotate 16 bit address incremented by Y value one bit the the right then ADC
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "RRA $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += Y;
            uint8_t old_carry = C;
            C = RAM[target_address] & 1;
            RAM[target_address] >>= 1;
            RAM[target_address] += (old_carry << 7);
            adc(RAM[target_address]);

            cycles += 7;

            PC++;
        }
        break;

        case 0x63:{
            //63 RRA (oper, X): Rotate indirect address indexed by X value one bit the the right then ADC
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            sprintf(assembly, "RRA ($%02X, X)", (uint8_t)(low - X));
            instruction = (instruction << 8) | (uint8_t)(low - X);
            log_information();
            uint8_t old_carry = C;
            C = RAM[effective_address] & 1;
            RAM[effective_address] >>= 1;
            RAM[effective_address] += (old_carry << 7);
            adc(RAM[effective_address]);

            cycles += 8;

            PC++;
        }
        break;

        case 0x73:{
            //73 RRA (oper), Y: Rotate indirect address indexed by Y value one bit the the right then ADC
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            sprintf(assembly, "RRA ($%02X), Y", (uint8_t)(low - X));
            instruction = (instruction << 8) | (uint8_t)(low - X);
            log_information();
            effective_address += Y;
            uint8_t old_carry = C;
            C = RAM[effective_address] & 1;
            RAM[effective_address] >>= 1;
            RAM[effective_address] += (old_carry << 7);
            adc(RAM[effective_address]);

            cycles += 8;

            PC++;
        }
        break;

        case 0x40:{
            //40 RTI: Return from interrupt
            sprintf(assembly, "RTI");
            log_information();

            P = RAM[0x100 + (++SP)];
            N = (P & 0x80) != 0;
            V = (P & 0x40) != 0;
            B = (P & 0x10) != 0;
            D = (P & 0x08) != 0;
            I = (P & 0x04) != 0;
            Z = (P & 0x02) != 0;
            C = (P & 0x01) != 0;
            uint16_t target_address = RAM[0x100 + (++SP)];
            target_address = (RAM[0x100 + (++SP)] << 8) | target_address;

            cycles += 6;

            PC = target_address;
        }
        break;


        case 0x60:{
            //60 RTS: Return to subroutine
            uint16_t target_address = RAM[0x100 + (++SP)];
            target_address = (RAM[0x100 + (++SP)] << 8) | target_address;
            sprintf(assembly, "RTS");
            log_information();

            cycles += 6;

            PC = target_address;
            PC++;
        }
        break;

        case 0xE9:{
            //E9 SBC #oper: Subtract immediate value with carry from accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "SBC #%02X", (uint8_t)operand);
            instruction = (instruction << 8) | (uint8_t)operand;
            log_information();
            sbc(operand);

            cycles += 2;

            PC++;
        }
        break;

        case 0xE5:{
            //E5 SBC zpg: Subtract zpg value with carry from accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "SBC $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            sbc(RAM[operand]);

            cycles += 3;

            PC++;
        }
        break;

        case 0xF5:{
            //F5 SBC zpg, X: Subtract zpg incremented by X value with carry from accumulator
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "SBC $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            sbc(RAM[operand]);

            cycles += 4;

            PC++;
        }
        break;

        case 0xED:{
            //ED SBC abs: Subtract 16bit address value with carry from accumulator
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "SBC $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            sbc(RAM[target_address]);

            cycles += 4;

            PC++;
        }
        break;

        case 0xFD:{
            //FD SBC abs, X: Subtract 16bit address incremented by X value with carry from accumulator
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "SBC $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + X) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += X;
            sbc(RAM[target_address]);

            PC++;
        }
        break;

        case 0xF9:{
            //F9 SBC abs, Y: Add 16bit address incremented by Y value with carry from accumulator
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "SBC $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            if(((target_address & 0xFF) + Y) > 255)
                cycles += 5;
            else
                cycles += 4;

            target_address += Y;
            sbc(RAM[target_address]);

            PC++;
        }
        break;

        case 0xE1:{
            //E1 SBC (oper, X): Subtract contents of word LL + X, LL + X + 1 from accumulator with carry
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            sprintf(assembly, "SBC ($%02X, X)", low - X);
            instruction = (instruction << 8) | (low - X);
            log_information();
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            sbc(RAM[effective_address]);

            cycles += 6;

            PC++;
        }
        break;

        case 0xF1:{
            //F1 SBC (oper), Y: Subtract contents of word (LL, LL + 1) + Y from accumulator with carry
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            sprintf(assembly, "SBC ($%02X), Y", low);
            instruction = (instruction << 8) | (low);
            log_information();
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];

            if(((effective_address & 0xFF) + Y) > 255)
                cycles += 6;
            else
                cycles += 5;

            effective_address += Y;
            sbc(RAM[effective_address]);

            PC++;
        }
        break;

        case 0x38:
            //38 SEC: Set carry flag
            sprintf(assembly, "SEC");
            log_information();
            C = 1;
            cycles += 2;
            update_status();
            PC++;
            break;

        case 0xF8:
            //F8 SED: Set decimal flag
            sprintf(assembly, "SED");
            log_information();
            D = 1;
            cycles += 2;
            update_status();
            PC++;
            break;

        case 0x78:
            //78 SEI: Set interrupt status
            sprintf(assembly, "SEI");
            log_information();
            I = 1;
            cycles += 2;
            update_status();
            PC++;
            break;


        case 0x87:{
            //87 SAX zpg: Store value from accumulator AND X into zpg
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "SAX $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();

            cycles += 3;

            RAM[operand] = A & X;
            PC++;
        }
        break;



        case 0x97:{
            //97 SAX zpg, Y: Store value from accumulator AND X in zpg with X offset
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "SAX $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += Y;

            cycles += 4;

            RAM[operand] = A & X;
            PC++;
        }
        break;


        case 0x8F:{
            //8F SAX abs: Store accumulator AND X in memory
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            instruction = instruction << 16 | target_address;
            sprintf(assembly, "SAX $%03X", target_address);
            log_information();
            RAM[target_address] = A & X;

            cycles += 4;

            PC++;
        }
        break;

        case 0x83:{
            //83 SAX ind, X: Effective address is LL + X, LL + X + 1
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            sprintf(assembly, "SAX ($%02X, X)", uint8_t(low - X));
            instruction = (instruction << 8) | (uint8_t)(low - X);
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();

            cycles += 6;

            RAM[effective_address] = A & X;
            PC++;
        }
        break;

        case 0x07:{
            //07 SLO zpg: Shift zpg one bit to the left then ORA
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "SLO $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            C = (RAM[operand] >> 7) & 1;
            RAM[operand] <<= 1;
            A |= RAM[operand];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 5;

            update_status();

            PC++;
        }
        break;

        case 0x17:{
            //17 SLO zpg, X: Shift zpg incremented by X one bit to the left then ORA
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "SLO $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            C = (RAM[operand] >> 7) & 1;
            RAM[operand] <<= 1;
            A |= RAM[operand];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x0F:{
            //0F SLO abs: Shift 16 bit address value one bit to the left then ORA
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "SLO $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            C = (RAM[target_address] >> 7) & 1;
            RAM[target_address] <<= 1;
            A |= RAM[target_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x1F:{
            //1F SLO abs, X: Shift 16 bit address incremented by X value one bit to the left then ORA
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "SLO $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;
            C = (RAM[target_address] >> 7) & 1;
            RAM[target_address] <<= 1;
            A |= RAM[target_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0x1B:{
            //1B SLO abs, Y: Shift 16 bit address incremented by Y value one bit to the left then ORA
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "SLO $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += Y;
            C = (RAM[target_address] >> 7) & 1;
            RAM[target_address] <<= 1;
            A |= RAM[target_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0x03:{
            //03 SLO (oper, X): Shift indirect address incremented by X value one bit to the left then ORA
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            sprintf(assembly, "SLO ($%02X, X)", (uint8_t)(low - X));
            instruction = (instruction << 8) | (uint8_t)(low - X);
            log_information();
            C = (RAM[effective_address] >> 7) & 1;
            RAM[effective_address] <<= 1;
            A |= RAM[effective_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 8;

            update_status();

            PC++;
        }
        break;

        case 0x13:{
            //13 SLO (oper), Y: Shift indirect address incremented by Y one bit to the left then ORA
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            sprintf(assembly, "SLO ($%02X), Y", (uint8_t)(low - X));
            instruction = (instruction << 8) | (uint8_t)(low - X);
            log_information();
            effective_address += Y;
            C = (RAM[effective_address] >> 7) & 1;
            RAM[effective_address] <<= 1;
            A |= RAM[effective_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 8;

            update_status();

            PC++;
        }
        break;

        case 0x47:{
            //47 SRE zpg: Shift value in zpg one bit to the right and EOR
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "SRE $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            C = RAM[operand] & 1;
            RAM[operand] >>= 1;
            A ^= RAM[operand];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 5;

            update_status();

            PC++;
        }
        break;

        case 0x57:{
            //57 SRE zpg, X: Shift value in zpg incremented by X one bit to the right then EOR
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "SRE $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;
            C = RAM[operand] & 1;
            RAM[operand] >>= 1;
            A ^= RAM[operand];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x4F:{
            //4F SRE abs: Shift value in 16 bit address one bit to the right then EOR
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "SRE $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            C = RAM[target_address] & 1;
            RAM[target_address] >>= 1;
            A ^= RAM[target_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 6;

            update_status();

            PC++;
        }
        break;

        case 0x5F:{
            //5F SRE abs, X: Shift value in 16 bit address incremented by X one bit to the right then EOR
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "SRE $%04X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;
            C = RAM[target_address] & 1;
            RAM[target_address] >>= 1;
            A ^= RAM[target_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0x5B:{
            //5B SRE abs, Y: Shift value in 16 bit address incremented by Y one bit to the right then EOR
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "SRE $%04X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += Y;
            C = RAM[target_address] & 1;
            RAM[target_address] >>= 1;
            A ^= RAM[target_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 7;

            update_status();

            PC++;
        }
        break;

        case 0x43:{
            //43 SRE (oper, X): Shift value in indirect address indexed by X one bit to the right then EOR
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            sprintf(assembly, "SRE ($%04X, X)", (uint8_t)(low - X));
            instruction = (instruction << 8) | (uint8_t)(low - X);
            log_information();
            C = RAM[effective_address] & 1;
            RAM[effective_address] >>= 1;
            A ^= RAM[effective_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 8;

            update_status();

            PC++;
        }
        break;

        case 0x53:{
            //53 SRE (oper), Y: Shift value in indirect address indexed by Y one bit to the right then EOR
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            sprintf(assembly, "SRE ($%04X), Y", (uint8_t)(low - X));
            instruction = (instruction << 8) | (uint8_t)(low - X);
            log_information();
            effective_address += Y;
            C = RAM[effective_address] & 1;
            RAM[effective_address] >>= 1;
            A ^= RAM[effective_address];
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 8;

            update_status();

            PC++;
        }
        break;


        case 0x85:{
            //85 STA zpg: Store value from accumulator into zpg
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "STA $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();

            cycles += 3;

            RAM[operand] = A;
            handleAPURegisters(operand, A);
            PC++;
        }
        break;



        case 0x95:{
            //95 STA zpg, X: Store value from accumulator in zpg with X offset
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "STA $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;

            cycles += 4;

            RAM[operand] = A;
            handleAPURegisters(operand, A);
            PC++;
        }
        break;


        case 0x8D:{
            //8D STA abs: Store accumulator in memory
            uint16_t target_address;
            target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            instruction = instruction << 16 | target_address;
            sprintf(assembly, "STA $%03X", target_address);
            log_information();
            RAM[target_address] = A;
            handleAPURegisters(target_address, A);

            switch(target_address){
                case 0x2000:
                    nes->getPPU().setCTRL(RAM[0x2000]);
                    t &= ~0b000110000000000;
                    t |= ((RAM[0x2000] & 0b11) << 10);
                    break;

                case 0x2001:
                    nes->getPPU().setMASK(RAM[0x2001]);
                    break;

                case 0x2002:
                    nes->getPPU().setSTATUS(RAM[0x2002]);
                    break;

                case 0x2003:
                    nes->getPPU().setOAMADDR(RAM[0x2003]);
                    break;

                case 0x2004:
                    nes->getPPU().setOAMDATA(RAM[0x2004]);
                    break;

                case 0x2005:
                    nes->getPPU().setSCROLL(RAM[0x2005]);
                    if(w == 0){

                        XSCROLL = RAM[0x2005];
                        x = RAM[0x2005] & 0b111;
                        t &= ~0b000000000011111;
                        t |= (RAM[0x2005] >> 3);
                        w = 1;

                    }else{
                        YSCROLL = RAM[0x2005];
                        t &= ~0b111001111100000;
                        t |= (RAM[0x2005] << 12);
                        t |= ((RAM[0x2005] >> 3) << 5);
                        w = 0;
                    }

                    break;

                case 0x2006:
                    nes->getPPU().setADDR(RAM[0x2006]);
                    if(w == 0){
                        t &= 0x00FF;
                        t |= (RAM[0x2006] & 0b111111) << 8;
                        w = 1;
                    }else{
                        t &= 0xFF00;
                        t |= (RAM[0x2006] & 0xFF);
                        VRAMADDR = t;
                        w = 0;

                    }
                    first2007 = true;

                    break;

                case 0x2007:


                        nes->getPPU().setDATA(RAM[0x2007]);
                        if((VRAMADDR >= 0x3F00 && VRAMADDR <= 0x3F1F) || (VRAMADDR >= 0x23C0 && VRAMADDR <= 0x23F7))
                            nes->getPPU().write(VRAMADDR, RAM[0x2007]);
                        else
                            nes->getPPU().write(0x2000 | (VRAMADDR & 0x0FFF), RAM[0x2007]);



                    if(VRAMADDR == 0x3F00){
                        nes->getPPU().write(0x3F10, RAM[0x2007]);

                    }

                    if(VRAMADDR == 0x3F10){
                        nes->getPPU().write(0x3F00, RAM[0x2007]);
                    }



                    nes->getPPU().write(0x3F04, nes->getPPU().getPPURAM()[0x3F00]);

                    nes->getPPU().write(0x3F08, nes->getPPU().getPPURAM()[0x3F00]);

                    nes->getPPU().write(0x3F0C, nes->getPPU().getPPURAM()[0x3F00]);

                    nes->getPPU().write(0x3F14, nes->getPPU().getPPURAM()[0x3F10]);

                    nes->getPPU().write(0x3F18, nes->getPPU().getPPURAM()[0x3F10]);

                    nes->getPPU().write(0x3F1C, nes->getPPU().getPPURAM()[0x3F10]);



                    if(nes->getMirrorType()){
                        if(VRAMADDR >= 0x2000 && VRAMADDR < 0x2800)
                            nes->getPPU().write(VRAMADDR + 0x800, RAM[0x2007]);
                        else if(VRAMADDR >= 0x2800 && VRAMADDR <= 0x2FFF)
                            nes->getPPU().write(VRAMADDR - 0x800, RAM[0x2007]);
                    }else{
                        if((VRAMADDR >= 0x2000 && VRAMADDR < 0x2400) || (VRAMADDR >= 0x2800 && VRAMADDR < 0x2C00))
                            nes->getPPU().write(VRAMADDR + 0x400, RAM[0x2007]);
                        else if((VRAMADDR >= 0x2400 && VRAMADDR < 0x2800) || (VRAMADDR >= 0x2C00 && VRAMADDR <= 0x2FFF))
                            nes->getPPU().write(VRAMADDR - 0x400, RAM[0x2007]);
                    }


                    if(scanlines >= -1 && scanlines <= 239 && (nes->getPPU().getMASK() & 0b1000 || nes->getPPU().getMASK() & 0b10000)){
                            if ((VRAMADDR & 0x001F) == 31){
                            VRAMADDR &= ~0x001F;
                            VRAMADDR ^= 0x0400;
                            }else{
                                VRAMADDR += 1;
                            }

                            if ((VRAMADDR & 0x7000) != 0x7000)
                              VRAMADDR += 0x1000;
                            else{
                              VRAMADDR &= ~0x7000;
                              int y = (VRAMADDR & 0x03E0) >> 5;
                              if (y == 29){
                                y = 0;
                                VRAMADDR ^= 0x0800;
                              }else if (y == 31)
                                y = 0;
                              else
                                y += 1;
                              VRAMADDR = (VRAMADDR & ~0x03E0) | (y << 5);
                            }

                        }else{
                            VRAMADDR += (nes->getPPU().getCTRL() & 0x04) ? 32 : 1;
                        }


                    break;


                case 0x4014:{
                    uint16_t sourceAddress = RAM[0x4014] << 8;
                    for(int i = RAM[0x2003]; i < 256; i++){
                        nes->getPPU().writeOAM(i, RAM[sourceAddress++]);
                    }
                    if(cycles % 2 != 0)
                        cycles += 514;
                    else
                        cycles += 513;
                    }
                    break;

                //PULSE 1 STARTS HERE
                case 0x4000:
                    nes->getAPU().p1.lengthCounter.halt = (RAM[target_address] >> 6) & 1;
                    nes->getAPU().p1.constantVolume = (RAM[target_address] >> 5) & 1;

                    switch(RAM[target_address] >> 6){
                        case 0:
                            //nes->getAPU().p1.dutySequence = {0, 0, 0, 0, 0, 0, 0, 1};
                            nes->getAPU().p1.dutySequence[7] = 1;
                        break;
                        case 1:
                            //nes->getAPU().p1.dutySequence = {0};
                            nes->getAPU().p1.dutySequence[7] = 1;
                            nes->getAPU().p1.dutySequence[6] = 1;
                        break;
                        case 2:
                            //nes->getAPU().p1.dutySequence = {0};
                            nes->getAPU().p1.dutySequence[7] = 1;
                            nes->getAPU().p1.dutySequence[6] = 1;
                            nes->getAPU().p1.dutySequence[5] = 1;
                            nes->getAPU().p1.dutySequence[4] = 1;
                        break;
                        case 3:
                            //nes->getAPU().p1.dutySequence = {0};
                            nes->getAPU().p1.dutySequence[0] = 1;
                            nes->getAPU().p1.dutySequence[1] = 1;
                            nes->getAPU().p1.dutySequence[2] = 1;
                            nes->getAPU().p1.dutySequence[3] = 1;
                            nes->getAPU().p1.dutySequence[4] = 1;
                            nes->getAPU().p1.dutySequence[5] = 1;
                        break;
                    }
                    nes->getAPU().p1.volumeP = RAM[target_address] & 0b1111;
                break;




                case 0x4002:
                    nes->getAPU().p1.lowRawP = RAM[target_address];
                break;

                case 0x4003:
                    nes->getAPU().p1.highRawP = RAM[target_address] & 0b111;

                    if(nes->getAPU().p1.enable)
                        nes->getAPU().p1.lengthCounterLoadP = nes->getAPU().lengthLookupTable[(RAM[target_address] >> 3)];

                    nes->getAPU().p1.raw = (RAM[target_address] << 8) | nes->getAPU().p1.lowRawP;
                    nes->getAPU().p1.phase = 0;


                break;

                //PULSE 2 STARTS HERE

                case 0x4004:
                    nes->getAPU().p2.lengthCounter.halt = (RAM[target_address] >> 6) & 1;
                    nes->getAPU().p2.constantVolume =  (RAM[target_address] >> 5) & 1;
                    switch(RAM[target_address] >> 6){
                        case 0:
                            //nes->getAPU().p1.dutySequence = {0, 0, 0, 0, 0, 0, 0, 1};
                            nes->getAPU().p2.dutySequence[7] = 1;
                        break;
                        case 1:
                            //nes->getAPU().p1.dutySequence = {0};
                            nes->getAPU().p2.dutySequence[7] = 1;
                            nes->getAPU().p2.dutySequence[6] = 1;
                        break;
                        case 2:
                            //nes->getAPU().p1.dutySequence = {0};
                            nes->getAPU().p2.dutySequence[7] = 1;
                            nes->getAPU().p2.dutySequence[6] = 1;
                            nes->getAPU().p2.dutySequence[5] = 1;
                            nes->getAPU().p2.dutySequence[4] = 1;
                        break;
                        case 3:
                            //nes->getAPU().p1.dutySequence = {0};
                            nes->getAPU().p2.dutySequence[0] = 1;
                            nes->getAPU().p2.dutySequence[1] = 1;
                            nes->getAPU().p2.dutySequence[2] = 1;
                            nes->getAPU().p2.dutySequence[3] = 1;
                            nes->getAPU().p2.dutySequence[4] = 1;
                            nes->getAPU().p2.dutySequence[5] = 1;
                        break;
                    }
                    nes->getAPU().p2.volumeP = RAM[target_address] & 0b1111;
                break;


                case 0x4006:
                    nes->getAPU().p2.lowRawP = RAM[target_address];
                break;

                case 0x4007:
                    nes->getAPU().p2.highRawP = RAM[target_address] & 0b111;

                    if(nes->getAPU().p2.enable)
                        nes->getAPU().p2.lengthCounterLoadP = nes->getAPU().lengthLookupTable[(RAM[target_address] >> 3)];

                    nes->getAPU().p2.raw = (RAM[target_address] << 8) | nes->getAPU().p2.lowRawP;
                    nes->getAPU().p2.phase = 0;


                break;


                case 0x4016:
                    //if(RAM[target_address] == 0x1){
                        controllerShift = nes->getPPU().getControllerState();
                        shiftCount = 0;
                //}

            }

            cycles += 4;

            PC++;
        }
        break;

        case 0x9D:{
            //9D STA abs, X:Stores accumulator at 16 bit address with X increment
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | (target_address);
            sprintf(assembly, "STA $%02X, X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += X;

            cycles += 5;

            RAM[target_address] = A;
            handleAPURegisters(target_address, A);
            PC++;
        }
        break;

        case 0x99:{
            //9D STA abs, Y:Stores accumulator at 16 bit address with Y increment
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "STA $%02X, Y", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();
            target_address += Y;

            cycles += 5;

            RAM[target_address] = A;
            handleAPURegisters(target_address, A);


            PC++;
        }
        break;

        case 0x81:{
            //81 STA ind, X: Effective address is LL + X, LL + X + 1
            uint8_t low = RAM[++PC] + X;
            uint8_t high = low + 1;
            sprintf(assembly, "STA ($%02X), X", low - X);
            instruction = (instruction << 8) | low - X;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            log_information();

            cycles += 6;

            RAM[effective_address] = A;
            handleAPURegisters(effective_address, A);
            PC++;
        }
        break;

        case 0x91:{
            //91 STA ind, Y: Effective address is LL, LL + 1
            uint8_t low = RAM[++PC];
            uint8_t high = low + 1;
            instruction = (instruction << 8) | low;
            uint16_t effective_address = (RAM[high] << 8) | RAM[low];
            effective_address += Y;
            sprintf(assembly, "STA ($%02X), Y", effective_address);
            log_information();




            cycles += 6;

            RAM[effective_address] = A;
            handleAPURegisters(effective_address, A);

            PC++;
        }
        break;

        case 0x86:{
            //86 STX zpg: Store X in zpg
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "STX $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();

            cycles += 3;

            RAM[operand] = X;
            PC++;
        }
        break;

        case 0x96:{
            //96 STX zpg, Y: Store X in zpg incremented by X
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "STX $%02X, Y", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand+=Y;

            cycles += 4;

            RAM[operand] = X;
            PC++;
        }
        break;

        case 0x8E:{
            //8E STX abs: Store X into 16 bit address
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "STX $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            cycles += 4;

            RAM[target_address] = X;
            PC++;
        }
        break;

        case 0x84:{
            //84 STY zpg: Store Y in zpg
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "STY $%02X", operand);
            instruction = (instruction << 8) | operand;
            log_information();

            cycles += 3;

            RAM[operand] = Y;
            PC++;
        }
        break;

        case 0x94:{
            //94 STY zpg, X: Store Y in zpg indexed by X
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "STY $%02X, X", operand);
            instruction = (instruction << 8) | operand;
            log_information();
            operand += X;

            cycles += 4;

            RAM[operand] = Y;
            PC++;
        }
        break;

        case 0x8C:{
            //8C STY abs: Store Y in 16 bit address
            uint16_t target_address = RAM[++PC];
            target_address = (RAM[++PC] << 8) | target_address;
            sprintf(assembly, "STY $%04X", target_address);
            instruction = (instruction << 16) | target_address;
            log_information();

            cycles += 4;

            RAM[target_address] = Y;
            PC++;
        }
        break;

        case 0xAA:
            //AA TAX: Transfer A to X
            sprintf(assembly, "TAX");
            log_information();
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 2;

            update_status();

            X = A;
            PC++;
            break;

        case 0xA8:
            //A8 TAY: Transfer A to Y
            sprintf(assembly, "TAY");
            log_information();
            N = (A & 128) != 0;
            Z = A == 0;

            cycles += 2;

            update_status();

            Y = A;
            PC++;
            break;

        case 0xBA:
            //BA TSX: Transfer SP to X
            sprintf(assembly, "TSX");
            log_information();
            N = (SP & 128) != 0;
            Z = SP == 0;

            cycles += 2;

            update_status();

            X = SP;
            PC++;
            break;

        case 0x8A:
            //8A TXA: Transfer X to A
            sprintf(assembly, "TXA");
            log_information();
            N = (X & 128) != 0;
            Z = X == 0;

            cycles += 2;

            update_status();

            A = X;
            PC++;
            break;


        case 0x9A:
            //9A TXS: Transfer X to SP
            sprintf(assembly, "TXS");
            log_information();
            SP = X;

            cycles += 2;

            PC++;
            break;

        case 0x98:
            //98 TYA: Transfer Y to accumulator
            sprintf(assembly, "TYA");
            log_information();
            N = (Y & 128) != 0;
            Z = Y == 0;

            cycles += 2;

            update_status();

            A = Y;
            PC++;
            break;

        case 0xEB:{
            //EB USBC: SBC immediate again for some reason
            uint8_t operand = RAM[++PC];
            sprintf(assembly, "USBC #%02X", (uint8_t)operand);
            instruction = (instruction << 8) | (uint8_t)operand;
            log_information();
            sbc(operand);

            cycles += 2;

            PC++;
        }
        break;

        default:
            sprintf(assembly, "----");
            log_information();
            PC++;
            break;
    }


    cyclesToExecute = cycles - cycles0;



    if(scanlines == nes->getPPU().hitScanline){
        RAM[0x2002] |= 0b01000000;
    }
    if(scanlines == -1){
        RAM[0x2002] &= ~0x80;
        RAM[0x2002] &= ~0b01000000;
    }



    if((nes->getPPU().getCTRL() & 0x80) && nes->getPPU().getSTATUS() & 0x80 && firstNMI){

        I = 1;
        B = 1;
        update_status();
        RAM[0x100 + SP] = (PC >> 8) & 0xFF;
        SP--;
        RAM[0x100 + SP] = PC & 0xFF;
        SP--;
        RAM[0x100 + SP] = (P);
        SP--;
        PC = NMI;
        firstNMI = false;
        cycles += 7;

        cyclesToExecute = cycles - cycles0;

    }






    endPC = PC;

}

