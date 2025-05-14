#include "emulator.h"

#include <span>
#include <ranges>
#include <vector>
#include <algorithm>

Emulator::Emulator(CPU& cpu, PPU& ppu)
    : cpu(cpu), ppu(ppu), frameDone(false)
{
    std::fill(rgbBuffer.begin(), rgbBuffer.end(), 0);
}

void Emulator::step() {
    // 1) Execute exactly one CPU clock (including any DMA stalls)
    int cpuClocks = cpu.tickCycle();

    // 2) For each CPU clock, run 3 PPU dots
    for (int i = 0; i < cpuClocks * 3; ++i) {
        ppu.stepDot();

        // 3) Detect end-of-frame: PPU has wrapped back to scanline 0, cycle 0
        if (ppu.getScanline() == 0 && ppu.getCycle() == 0) {
            frameDone = true;
            break;
        }
    }
}

CPU* Emulator::getCPU() const { return &cpu; }
PPU* Emulator::getPPU() const { return &ppu; }

bool Emulator::frameComplete() const {
    return frameDone;
}

void Emulator::resetFrameFlag() {
    frameDone = false;
}

const uint8_t* Emulator::getFrameBuffer() const {
    return ppu.getFrameBuffer().data();
}

const uint32_t* Emulator::getRgbFrame() {
    // 1) Grab the 8-bit palette indices from the PPU
    const uint8_t* idxFb = ppu.getFrameBuffer().data();

    // 2) Map each index → 32-bit color via the NES_SYSTEM_PALETTE
    std::transform(
        idxFb,
        idxFb + rgbBuffer.size(),
        rgbBuffer.begin(),
        [](uint8_t i) {
            // mask to 0–63 just in case
            return NES_SYSTEM_PALETTE[i & 0x3F];
        }
    );

    // 3) Return the raw RGBA buffer for your renderer
    return rgbBuffer.data();
}