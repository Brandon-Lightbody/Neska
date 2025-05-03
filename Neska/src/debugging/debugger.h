#pragma once

#include <cstdint>
#include <vector>
#include <unordered_set>
#include <memory>

#include <SDL3/SDL.h>
#include "emulator.h"
#include "memory_bus.h"

struct CPUState {
    uint16_t PC;
    uint8_t  A, X, Y, SP, status;
};

struct PPUState {
    int scanline;
    int cycle;

    // Internal registers for loopy-V/X/Y scrolling
    uint16_t v, t;
    uint8_t fineX;
    uint8_t ppuCtrl, ppuMask;
    bool    nmiOccurred, vblank, sprite0Hit, spriteOverflow;
};

class DebugGUI;  // forward

class Debugger {
public:
    Debugger(Emulator& emu, MemoryBus& mem);
    ~Debugger();

    // ImGui lifecycle (all inside DebugGUI)
    void initGui(SDL_Window* window, SDL_Renderer* renderer);
    void newFrameGui();
    void drawGui();
    void renderGui();
    void shutdownGui();

    // Emulation control
    void update();        // handles pause/step/breakpoints

    /// single-step the CPU (pauses again after one tick)
    void requestStep();

    void pause();
    void resume();

    bool isPaused() const;

    // Breakpoints
    void toggleBreakpoint(uint16_t addr);
    bool hasBreakpoint(uint16_t addr) const;
    const std::unordered_set<uint16_t>& getBreakpoints() const;

    // State accessors for GUI
    CPUState getCPUState() const;
    PPUState getPPUState() const;
    std::vector<uint8_t> peekMemory(uint16_t addr, size_t len) const;
    std::vector<uint8_t> peekPPUMemory(uint16_t addr, size_t len) const;

private:
    Emulator& emu;
    MemoryBus& mem;
    CPU& cpu;
    PPU& ppu;

    bool paused;
    bool stepRequested;
    std::unordered_set<uint16_t> breakpoints;

    std::unique_ptr<DebugGUI> gui;
};
