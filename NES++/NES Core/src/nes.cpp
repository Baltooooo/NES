#include "nes.h"
#include <iostream>
#include <vector>
#include <stdio.h>
#include <string.h>
using namespace std;


NES::NES(){
    this->CPU = new NES_CPU();
    CPU->connectToNES(this);
    this->PPU = new NES_PPU();
    PPU->connectToNES(this);
    this->APU = new NES_APU();
    APU->connectToNES(this);

}

size_t PRGSize;

void NES::write(uint16_t address, uint8_t value){
    RAM[address] = value;
}

void NES::readConfig(){
    ifstream config("NES-config.txt", ios::in);
    string line;

    while(getline(config, line)){
        if(line.rfind("romPath=", 0) == 0){
            romPath = line.substr(8);
        }
        if(line.rfind("mute=", 0) == 0){
            muted = (line.substr(5) == "true") ? true : false;
        }
        if(line.rfind("fullscreen=", 0) == 0){
            fullscreen = (line.substr(11) == "true") ? true : false;
            PPU->selectFullscreen();
        }
        if(line.rfind("logCPU=", 0) == 0){
            logCPU = (line.substr(7) == "true") ? true: false;
        }
    }
}

void NES::loadROM(){
    readConfig();
    ifstream romFile(romPath, ios::binary);
    romFile.seekg(0, ios::end);
    size_t rom_size = romFile.tellg();
    romFile.seekg(0, ios::beg);
    uint8_t header[16];
    romFile.read(reinterpret_cast<char*>(header), sizeof(header));
    PRGSize = header[4] * 16 * 1024;
    nametableMirroring = header[6] & 1;

    if(nametableMirroring)
        cout << "Using vertical nametable mirroring..." << endl;
    else
        cout << "Using horizontal nametable mirroring..." << endl;

    size_t CHRSize = header[5] * 8 * 1024;
    size_t mapperNumber = (header[6] >> 4) | (header[7] & 0xF0);


    vector<uint8_t> PRGData(PRGSize);

    romFile.read(reinterpret_cast<char*>(PRGData.data()), PRGSize);

    if(PRGSize == 0x4000){
        memcpy(RAM + 0x8000, PRGData.data(), 0x4000);
        memcpy(RAM + 0xC000, PRGData.data(), 0x4000);
        CPU->setPC(0x8000);
    }else if(PRGSize == 0x8000){
        memcpy(RAM + 0x8000, PRGData.data(), 0x8000);
        CPU->setPC(0x8000);
    }else{
        cout << "ERROR" << endl;
    }

    FILE *ramMap = fopen("RAM.txt", "w");

    for(int i = 0x8000; i < 0xC000; i++){
        fprintf(ramMap, "%04X: %02X \n", i, RAM[i]);
    }

    mapper(mapperNumber);



    uint16_t resetVector = (RAM[0xFFFD] << 8) | RAM[0xFFFC];
    CPU->setPC(resetVector);

    vector<uint8_t> CHRData(CHRSize);

    romFile.read(reinterpret_cast<char*>(CHRData.data()), CHRSize);



    if(CHRSize != 0x00)
        memcpy(PPU->getPPURAM(), CHRData.data(), CHRSize);
    else
        memcpy(PPU->getPPURAM(), 0, 8 * 1024);


    romFile.close();

    cout << "ROM Loaded" << endl;




}

void NES::mapper(uint8_t mapperNumber){
    switch(mapperNumber){

        case 0:{
            //Mapper 0
            cout << "Using Mapper 0..." << endl;
            if(PRGSize == 0x4000){
                CPU->setNMI((RAM[0xBFFB] << 8) | RAM[0xBFFA]);
            }else{
                CPU->setNMI((RAM[0xFFFB] << 8) | RAM[0xFFFA]);
            }

        }

    }
}
