#pragma once

#include "cpu.h"
#include "ppu.h"

// Drives CPU and PPU in lockstep: 1 CPU clock = 3 PPU dots,
// handles NMI wiring, DMA stalls, and frame completion.
class Emulator {
public:
    Emulator(CPU& cpu, PPU& ppu);

    // Advance exactly one CPU clock (and its 3 PPU dots).
    // Must be called repeatedly to run the emulation.
    void step();

    // Did we just finish a frame?  (i.e. PPU wrapped to scanline 0,cyle 0)
    bool frameComplete() const;

    // Clear the “just finished a frame” flag so you can draw again.
    void resetFrameFlag();

    // Grab the latest 256×240 ARGB frame buffer from the PPU
    const uint32_t* getFrameBuffer() const;

private:
    CPU& cpu_;
    PPU& ppu_;
    bool frameDone_;
};
