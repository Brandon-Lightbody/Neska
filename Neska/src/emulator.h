#pragma once

#include "cpu.h"
#include "ppu.h"
#include "core.h"

class Emulator {
public:
    Emulator(CPU& cpu, PPU& ppu);

    void step();

    CPU* getCPU() const;
    PPU* getPPU() const;

    bool frameComplete() const;

    void resetFrameFlag();

    const uint8_t* getFrameBuffer() const;
private:
    CPU& cpu;
    PPU& ppu;
    bool frameDone;
};
