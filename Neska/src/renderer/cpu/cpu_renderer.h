#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include "memory_bus.h"

class Renderer {
public:
    Renderer(int w, int h, const std::string& title);
    ~Renderer();

    bool pollEvents(MemoryBus& memory);
    void processDebugGuiEvents(SDL_Event event);

    void transformPixelBuffer(const uint8_t* idxBuffer);
    void upscaleImage(const uint8_t* source, int sw, int sh, int scale);
    void renderFrame();
    void presentFrame();
    void clearPixelBuffer();
    
    SDL_Window* getSDLWindow();
    SDL_Renderer* getSDLRenderer();
private:
    SDL_Window* sdlWindow;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* texture;
    int width;
    int height;

    std::vector<uint32_t> pixelBuffer;
};
