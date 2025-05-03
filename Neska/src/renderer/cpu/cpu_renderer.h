#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include "memory_bus.h"

class Renderer {
public:
    Renderer(int w, int h, const std::string& title);
    ~Renderer();

    void renderFrame(const uint32_t* pixels);
    void present();
    bool pollEvents(MemoryBus& memory);

    std::vector<uint32_t> upscaleImage(const uint32_t* source, int sw, int sh, int scale);

    SDL_Window* getSDLWindow();
    SDL_Renderer* getSDLRenderer();

private:
    SDL_Window* sdlWindow;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* texture;
    int width, height;
};
