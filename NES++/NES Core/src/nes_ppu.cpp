#include "nes_ppu.h"
#include <iostream>

using namespace std;

SDL_Joystick* joysticks[2];
uint8_t lastHat = 0;
bool isMoving() {lastHat != 0;}
uint8_t background[240][256];


void NES_PPU::selectFullscreen(){
    if(nes->fullscreen){
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
        SDL_Rect displayBounds;
        if(SDL_GetDisplayBounds(0, &displayBounds) == 0){
            screenWidth = displayBounds.w;
            screenHeight = displayBounds.h;
        }
    }else{
        SDL_SetWindowFullscreen(window, 0);
    }

}
NES_PPU::NES_PPU(){


    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) != 0){
        SDL_Log("Failed to initialize subsystems.", SDL_GetError());
        exit(0);
    }


    cout << "Number of joysticks plugged in: " << SDL_NumJoysticks() << endl;


    SDL_JoystickEventState(SDL_ENABLE);

    for(int i = 0; i < SDL_NumJoysticks(); i++){
        joysticks[i] = SDL_JoystickOpen(i);
        cout << "Joystick #" << i + 1 << " opened..." << endl;
    }

    window = SDL_CreateWindow("NES++", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               screenWidth * screenMult * 8, screenHeight * screenMult * 8, 0);


    if(!window){
        SDL_Log("Failed to create window.", SDL_GetError());
        exit(0);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if(!renderer){
        SDL_Log("Failed to create renderer.", SDL_GetError());
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);

}

void NES_PPU::cleanup(){
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}



void NES_PPU::inputHandler(){
    SDL_Event event;

    while(SDL_PollEvent(&event)){
        switch(event.type){

            //Keyboard =====================================================================================

            case SDL_KEYDOWN:
                switch(event.key.keysym.sym){
                    case SDLK_ESCAPE:
                        emulatorState = QUIT;
                        break;
                    case SDLK_RIGHT:
                        controllerState |= 0b10000000;
                        break;
                    case SDLK_LEFT:
                        controllerState |= 0b01000000;
                        break;
                    case SDLK_DOWN:
                        controllerState |= 0b00100000;
                        break;
                    case SDLK_UP:
                        controllerState |= 0b00010000;
                        break;
                    case SDLK_RETURN:
                        controllerState |= 0b00001000;
                        break;
                    case SDLK_BACKSPACE:
                        controllerState |= 0b00000100;
                        break;
                    case SDLK_x:
                        controllerState |= 0b00000010;
                        break;
                    case SDLK_z:
                        controllerState |= 0b00000001;
                        break;
                }
                break;


            case SDL_KEYUP:
                switch(event.key.keysym.sym){
                    case SDLK_RIGHT:
                        controllerState &= ~0b10000000;
                        break;
                    case SDLK_LEFT:
                        controllerState &= ~0b01000000;
                        break;
                    case SDLK_DOWN:
                        controllerState &= ~0b00100000;
                        break;
                    case SDLK_UP:
                        controllerState &= ~0b00010000;
                        break;
                    case SDLK_RETURN:
                        controllerState &= ~0b00001000;
                        break;
                    case SDLK_BACKSPACE:
                        controllerState &= ~0b00000100;
                        break;
                    case SDLK_x:
                        controllerState &= ~0b00000010;
                        break;
                    case SDLK_z:
                        controllerState &= ~0b00000001;
                        break;
                }
                break;

                //Joystick ================================================================


                case SDL_JOYHATMOTION:{

                    if(!isMoving()){
                        uint8_t hat = SDL_JoystickGetHat(joysticks[0], event.jhat.hat);

                        if(hat == 0){
                            controllerState &= ~0b11110000;
                            break;
                        }

                        if(hat & SDL_HAT_RIGHT){
                            controllerState |= 0b10000000;
                        }else{
                            controllerState &=  ~0b10000000;
                        }
                        if(hat & SDL_HAT_LEFT){
                            controllerState |= 0b01000000;
                        }else{
                            controllerState &=  ~0b01000000;
                        }
                        if(hat & SDL_HAT_DOWN){
                            controllerState |= 0b00100000;
                        }else{
                            controllerState &=  ~0b00100000;
                        }
                        if(hat & SDL_HAT_UP){
                            controllerState |= 0b00010000;
                        }else{
                            controllerState &=  ~0b00010000;
                        }
                        lastHat = hat;
                    }else{
                        break;
                    }
                }
                break;


                case SDL_JOYBUTTONDOWN:
                        switch(event.jbutton.button){
                            case 0x09:
                                controllerState |= 0b00001000;
                            break;
                            case 0x08:
                                controllerState |= 0b00000100;
                            break;
                            case SDL_CONTROLLER_BUTTON_Y:
                                controllerState |= 0b00000010;
                            break;
                            case SDL_CONTROLLER_BUTTON_X:
                                controllerState |= 0b00000001;
                            break;
                        }
                        break;

                    case SDL_JOYBUTTONUP:
                        switch(event.jbutton.button){
                            case 0x09:
                                controllerState &= ~0b00001000;
                            break;
                            case 0x08:
                                controllerState &= ~0b00000100;
                            break;
                            case SDL_CONTROLLER_BUTTON_Y:
                                controllerState &= ~0b00000010;
                            break;
                            case SDL_CONTROLLER_BUTTON_X:
                                controllerState &= ~0b00000001;
                            break;
                        }
                        break;


        }
    }

}
int nametableState = 0;
uint16_t nametable = 0x2000;
uint32_t framebuffer[256*240];


void NES_PPU::present(){
        SDL_Rect dest = {
        0,
        0,
        (nes->fullscreen) ? screenWidth : (256 * screenMult),
        (nes->fullscreen) ? screenHeight : (240 * screenMult),
        };
        SDL_RenderClear(renderer);
        SDL_UpdateTexture(texture, nullptr, framebuffer, 256 * sizeof(uint32_t));
        SDL_RenderCopy(renderer, texture, nullptr, &dest);
        SDL_RenderPresent(renderer);
}




void NES_PPU::renderPixel(int scanlines, int dot, int pixel, int pallete){

    int palleteBase;

    switch(pallete){
        case 0:
            palleteBase = 0x3F00;
        break;
        case 1:
            palleteBase = 0x3F04;
        break;
        case 2:
            palleteBase = 0x3F08;
        break;
        case 3:
            palleteBase = 0x3F0C;
        break;

    }

    int colorIndex = PPU_RAM[palleteBase + pixel];

    framebuffer[dot + scanlines * 256] = systemPallete[colorIndex];
}



bool NES_PPU::isSprite0(uint8_t spriteY, uint16_t spriteTile, uint16_t spriteAttribute, uint8_t spriteX){
    return OAM[0] == spriteY && OAM[1] == spriteTile && OAM[2] == spriteAttribute && OAM[3] == spriteX;
}

void NES_PPU::renderSprite(int scanlines, int spriteX, uint8_t MSB, uint8_t LSB, uint16_t pallete, bool isSprite0){

    uint16_t palleteBase;
    bool horizontalFlip = (pallete >> 6) & 1;

    switch(pallete & 0b11){
        case 0:
            palleteBase = 0x3F10;
        break;
        case 1:
            palleteBase = 0x3F14;
        break;
        case 2:
            palleteBase = 0x3F18;
        break;
        case 3:
            palleteBase = 0x3F1C;
        break;

    }

    for(int i = 0; i < 8; i++){
        int I = horizontalFlip ? (7 - i) : i;
        int pixel = (((MSB >> (7 - I)) & 1) << 1) | ((LSB >> (7 - I)) & 1);

        int colorIndex = PPU_RAM[palleteBase + pixel];


        if(pixel != 0){

            if(isSprite0 && framebuffer[spriteX - 8 + i + scanlines * 256] != systemPallete[PPU_RAM[0x3F00]]){
                hitScanline = scanlines;
            }

            if(framebuffer[spriteX - 8 + i + scanlines * 256] != systemPallete[PPU_RAM[0x3F00]] && ((pallete >> 5) & 1) == 1){
                //Do not render
            }else{
                framebuffer[spriteX - 8 + i + scanlines * 256] = systemPallete[colorIndex];
            }
        }



    }

}

void NES_PPU::renderLoopy(int scanlines, int dot, uint8_t MSB, uint8_t LSB, uint16_t pallete){

    int fineX = nes->getCPU().x;

    for(int i = 0; i < 8; i++){
        int pixel = (((MSB >> (7 - i)) & 1) << 1) | ((LSB >> (7 - i)) & 1);
        uint16_t palleteBase;
        int value;

        int coarseX = nes->getCPU().XSCROLL >> 3;


        uint8_t bottomRight = PPU_RAM[pallete] >> 6;
        uint8_t bottomLeft = (PPU_RAM[pallete] >> 4) & 0b11;
        uint8_t topRight = (PPU_RAM[pallete] >> 2) & 0b11;
        uint8_t topLeft = (PPU_RAM[pallete]) & 0b11;


        if((scanlines % 32) > 15 && ((dot - 8) % 32) > 15)
            value = bottomRight;
        else if((scanlines % 32) > 15 && ((dot - 8) % 32) <= 15)
            value = bottomLeft;
        else if((scanlines % 32) <= 15 && ((dot - 8) % 32) <= 15)
            value = topLeft;
        else if((scanlines % 32) <= 15 && ((dot - 8) % 32) > 15)
            value = topRight;


        switch(value){
            case 0:
                palleteBase = 0x3F00;
            break;
            case 1:
                palleteBase = 0x3F04;
            break;
            case 2:
                palleteBase = 0x3F08;
            break;
            case 3:
                palleteBase = 0x3F0C;
            break;

        }



        int colorIndex = PPU_RAM[palleteBase + pixel];

        framebuffer[dot - 8 + i + scanlines * 256] = systemPallete[colorIndex];

    }


}




void NES_PPU::logPPU(){

    FILE* ppuLog = fopen("PPU-Log.txt", "w");
    for(int i = 0; i < 16384; i++){
        fprintf(ppuLog, "%02X: %04X \n", i, PPU_RAM[i]);
    }
    fprintf(ppuLog, "OAM Data: \n");
    for(int i = 0; i < 256; i++){
        fprintf(ppuLog, "%04X: %02X \n", i, OAM[i]);
    }
    fprintf(ppuLog, "Secondary OAM Data: \n");
    for(int i = 0; i < 32; i++){
        fprintf(ppuLog, "%04X: %02X \n", i, secondaryOAM[i]);
    }
    fprintf(ppuLog, "Pallete per tile: \n | ");
    for(int i = 0, k = 1; i < 32*30; i++, k++){
        fprintf(ppuLog, "%02X ", framePalletes[i]);
        if((k % 2 == 0))
            fprintf(ppuLog, " | ");
        if(k % 32 == 0)
            fprintf(ppuLog, "\n | ");
        if(k % 64 == 0)
            fprintf(ppuLog, "-----------------------------------------------------------------------------------------------------------------------------------------------\n | ");
    }
    fprintf(ppuLog, "Background/Foreground: \n");
    for(int i = 0; i < 240; i++){
        for(int j = 0; j < 256; j++){
            fprintf(ppuLog, "%02X ", framebuffer[i + (j*240)]);
        }
        fprintf(ppuLog, "\n");
    }

}
