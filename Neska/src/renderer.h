#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include "memory.h"

class Renderer {
public:
    Renderer(int w, int h, const std::string& title);
    ~Renderer();

    void renderFrame(const uint32_t* pixels);
    bool pollEvents(Memory& memory);

    std::vector<uint32_t> upscaleImage(const uint32_t* source, int sw, int sh, int scale);

private:
    SDL_Window* window;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* texture;
    int width, height;
};
