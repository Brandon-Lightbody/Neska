#include "cpu_renderer.h"
#include <iostream>
#include <cassert>
#include <imgui_impl_sdl3.h>

Renderer::Renderer(int w, int h, const std::string& title)
    : sdlWindow(nullptr), sdlRenderer(nullptr), texture(nullptr),
    width(w), height(h)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL init error:" << SDL_GetError() << "\n";
        exit(1);
    }
    sdlWindow = SDL_CreateWindow(title.c_str(), width, height, 0);
    if (!sdlWindow) {
        std::cerr << "Window creation error:" << SDL_GetError() << "\n";
        SDL_Quit();
        exit(1);
    }
    sdlRenderer = SDL_CreateRenderer(sdlWindow, 0);
    if (!sdlRenderer) {
        std::cerr << "Renderer creation error:" << SDL_GetError() << "\n";
        SDL_DestroyWindow(sdlWindow);
        SDL_Quit();
        exit(1);
    }
    texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!texture) {
        std::cerr << "Texture creation error:" << SDL_GetError() << "\n";
        SDL_DestroyRenderer(sdlRenderer);
        SDL_DestroyWindow(sdlWindow);
        SDL_Quit();
        exit(1);
    }
}

Renderer::~Renderer()
{
    if (texture)SDL_DestroyTexture(texture);
    if (sdlRenderer)SDL_DestroyRenderer(sdlRenderer);
    if (sdlWindow)SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
}

void Renderer::renderFrame(const uint32_t* pixels)
{
    SDL_UpdateTexture(texture, nullptr, pixels, width * sizeof(uint32_t));
    SDL_RenderClear(sdlRenderer);
    SDL_RenderTexture(sdlRenderer, texture, nullptr, nullptr);
}

void Renderer::present() {
    SDL_RenderPresent(sdlRenderer);
}

bool Renderer::pollEvents(MemoryBus& memory)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL3_ProcessEvent(&e);
        if (e.type == SDL_EVENT_QUIT) {
            return false;
        }
        else if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.scancode) {
            case SDL_SCANCODE_Z: memory.setButtonPressed(0); break;
            case SDL_SCANCODE_X: memory.setButtonPressed(1); break;
            case SDL_SCANCODE_RETURN: memory.setButtonPressed(3); break;
            case SDL_SCANCODE_RSHIFT: memory.setButtonPressed(2); break;
            case SDL_SCANCODE_UP: memory.setButtonPressed(4); break;
            case SDL_SCANCODE_DOWN: memory.setButtonPressed(5); break;
            case SDL_SCANCODE_LEFT: memory.setButtonPressed(6); break;
            case SDL_SCANCODE_RIGHT: memory.setButtonPressed(7); break;
            default: break;
            }
        }
        else if (e.type == SDL_EVENT_KEY_UP) {
            switch (e.key.scancode) {
            case SDL_SCANCODE_Z: memory.clearButtonPressed(0); break;
            case SDL_SCANCODE_X: memory.clearButtonPressed(1); break;
            case SDL_SCANCODE_RETURN: memory.clearButtonPressed(3); break;
            case SDL_SCANCODE_RSHIFT: memory.clearButtonPressed(2); break;
            case SDL_SCANCODE_UP: memory.clearButtonPressed(4); break;
            case SDL_SCANCODE_DOWN: memory.clearButtonPressed(5); break;
            case SDL_SCANCODE_LEFT: memory.clearButtonPressed(6); break;
            case SDL_SCANCODE_RIGHT: memory.clearButtonPressed(7); break;
            default: break;
            }
        }
    }
    return true;
}

std::vector<uint32_t> Renderer::upscaleImage(const uint32_t* source, int sw, int sh, int scale)
{
    int dw = sw * scale;
    int dh = sh * scale;    

    std::vector<uint32_t> out(dw * dh);
    for (int y = 0; y < dh; y++) {
        int sy = y / scale;
        for (int x = 0; x < dw; x++) {
            int sx = x / scale;
            out[y * dw + x] = source[sy * sw + sx];

            if (sx >= sw || sy >= sh) {
                std::cout << "Overflow at upscale (sx=" << sx << ", sy=" << sy << ")\n";
            }
        }
    }

    assert(sw == 256 && sh == 240);
    
    return out;
}

SDL_Window* Renderer::getSDLWindow() { return sdlWindow; }
SDL_Renderer* Renderer::getSDLRenderer() { return sdlRenderer; }