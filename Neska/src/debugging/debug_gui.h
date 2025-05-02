#pragma once

#include <cstdint>
#include <SDL3/SDL.h>
#include <imgui.h>

class Debugger;

class DebugGUI {
public:
    explicit DebugGUI(Debugger& dbg);
    ~DebugGUI();

    // ImGui lifecycle
    void init(SDL_Window* window, SDL_Renderer* renderer);
    void newFrame();
    void draw();
    void render();
    void shutdown();

private:
    Debugger& dbg;
    uint16_t memViewAddr;
    SDL_Window* sdlWindow;
    SDL_Renderer* sdlRenderer;
};
