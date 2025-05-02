#include "emulator.h"

Emulator::Emulator(CPU& cpu, PPU& ppu)
    : cpu(cpu), ppu(ppu), frameDone(false)
{
}

void Emulator::step() {
    // 1) Execute exactly one CPU clock (including any DMA stalls)
    int cpuClocks = cpu.tickCycle();

    // 2) For each CPU clock, run 3 PPU dots
    for (int i = 0; i < cpuClocks * 3; ++i) {
        ppu.stepDot();

        // As soon as the PPU raises NMI (and PPUCTRL bit 7 was set), queue it
        if (ppu.isNmiTriggered()) {
            cpu.requestNmi();
            ppu.clearNmiFlag();
        }

        // 3) Detect end-of-frame: set once and exit the loop
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

const uint32_t* Emulator::getFrameBuffer() const {
    return ppu.getFrameBuffer();
}