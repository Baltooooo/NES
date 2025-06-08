#ifndef NES_APU_H
#define NES_APU_H
#include <iostream>
#include <vector>
#include <SDL2/SDL.h>

using namespace std;

class NES;

struct FrameCounter{
    bool mode; //0 - 4 step, 1 - 5 step
    bool interruptFlag = 0;
    int count = 0;
};

struct LengthCounter{
    bool halt;
    int count = 0;
};

struct LinearCounter{
    bool reloadFlag;
    uint8_t linearCounterLoad;
    int count = 0;
};


struct Pulse{
    LengthCounter lengthCounter;
    uint8_t phase;
    uint8_t dutySequence[8];
    uint8_t volumeP;
    uint8_t lowRawP = 0;
    uint8_t highRawP = 0;
    int lengthCounterLoadP;
    uint16_t raw;
    bool constantVolume;
    bool enable;

    float getFreqP(){
    return 1789773 / (16. * (raw + 1));
    }

};

struct Triangle{
    LinearCounter linearCounter;
    LengthCounter lengthCounter;
    uint8_t lengthCounterLoadT;
    uint8_t freqT;
    uint8_t lowRawT;
    uint8_t highRawT;
    uint16_t timerLoad;
    uint16_t raw;
    int sequenceIndex = 0;
    int sequenceValue = 15;
    bool enable;
    float getFreqP(){
    return 1789773 / (32 * (raw + 1));
    }
};

struct Noise{
    uint8_t volumeN;
    uint8_t toneMode;
    uint8_t period;
};



class NES_APU
{
    private:
        NES* nes;
        int apuCycles = 4;
        float lastSample = 0.0f;
        FrameCounter fc;
        Pulse p1;
        Pulse p2;
        Triangle triangle;
        vector<int16_t> audioBuffer;
        SDL_AudioSpec want = {};
        SDL_AudioDeviceID audioDev;
        uint8_t lengthLookupTable[32] = {
        10, 254, 20, 2, 40, 4, 80, 6, 160,
        8, 60, 10, 14, 12, 26, 14, 12, 16,
        24, 18, 48, 20, 96, 22, 192, 24, 72,
        26, 16, 28, 32, 30
        };
        uint8_t triangleWavelengthTable[32] = {
        15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
        0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
        };

    public:
        NES_APU();
        void connectToNES(NES* ptrNES) {this->nes = ptrNES;}
        void step(int amount);
        void clockLinearCounter();
        void clockTriangle();


    friend class NES_CPU;
};

#endif // NES_APU_H
