#include "emulator.h"

Emulator::Emulator(CPU& cpu, PPU& ppu)
    : cpu_(cpu), ppu_(ppu), frameDone_(false)
{
}

void Emulator::step() {
    // 1) Execute exactly one CPU clock (including any DMA stalls)
    int cpuClocks = cpu_.tickCycle();

    // 2) For each CPU clock, run 3 PPU dots
    for (int i = 0; i < cpuClocks * 3; ++i) {
        ppu_.stepDot();
        // As soon as the PPU raises NMI (and PPUCTRL bit 7 was set),
        // queue it into the CPU
        if (ppu_.isNmiTriggered()) {
            cpu_.requestNmi();
            ppu_.clearNmiFlag();
        }
    }

    // 3) Detect end‑of‑frame (PPU just wrapped to scanline=0, cycle=0)
    frameDone_ = (ppu_.getScanline() == 0 && ppu_.getCycle() == 0);
}

bool Emulator::frameComplete() const {
    return frameDone_;
}

void Emulator::resetFrameFlag() {
    frameDone_ = false;
}

const uint32_t* Emulator::getFrameBuffer() const {
    return ppu_.getFrameBuffer();
}