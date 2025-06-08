#include "nes_apu.h"
#include <vector>
#include <iostream>

using namespace std;


NES_APU::NES_APU(){
    want.channels = 1;
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.samples = 1024;
    audioDev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (!audioDev) {
        exit(1);
    }else{
        cout << "Audio Device Successfully Started..." << endl;
    }
    SDL_PauseAudioDevice(audioDev, 0);
}

void NES_APU::step(int amount){

    for(int i = 0; i < amount; i++){
        if(i % 2 == 0)
            apuCycles++;
        if(apuCycles % 3728 == 0){
            if(!fc.mode){
                fc.count = (++fc.count) % 5;
                if(fc.count == 2 || fc.count == 4){
                    if(triangle.lengthCounter.count > 0)
                        triangle.lengthCounter.count--;
                }
                if(fc.count == 4)
                    fc.interruptFlag = true;
            }else{
                fc.count = (++fc.count) % 6;
                if(fc.count == 2 || fc.count == 5){
                    if(triangle.lengthCounter.count > 0 && !triangle.lengthCounter.halt)
                        triangle.lengthCounter.count--;
                }
            }
            if (fc.count != 0)
                clockLinearCounter();
        }
        if(i % 2 == 0){
            p1.raw--;
            p1.phase = (p1.phase + 1) % 8;
            p2.raw--;
        }
        triangle.raw--;
        if(triangle.raw == 0){
            triangle.raw = triangle.timerLoad + 1;

            if(triangle.linearCounter.count != 0 && triangle.lengthCounter.count != 0){
                triangle.sequenceValue = triangleWavelengthTable[(++triangle.sequenceIndex) % 32];
            }
        }
        if(apuCycles % 25 == 0){
            if(triangle.enable && triangle.linearCounter.count > 0 && triangle.lengthCounter.count > 0){
                float sample = (triangle.sequenceValue / 15.0f);
                int16_t triangleSample = static_cast<int16_t>(sample * 32767);
                audioBuffer.push_back(triangleSample);
            }
        }
        if(audioBuffer.size() == 800){

            SDL_QueueAudio(audioDev, audioBuffer.data(), audioBuffer.size() * sizeof(int16_t));
            audioBuffer.clear();
        }



    }
}

void NES_APU::clockLinearCounter(){
    if(triangle.linearCounter.reloadFlag){
        triangle.linearCounter.count = triangle.linearCounter.linearCounterLoad;
    }
    else if(triangle.linearCounter.count > 0)
        triangle.linearCounter.count--;
    if(!triangle.lengthCounter.halt)
        triangle.linearCounter.reloadFlag = false;

}
