#include "emulator.h"

#include <span>
#include <ranges>
#include <vector>
#include <algorithm>

Emulator::Emulator(CPU& cpu, PPU& ppu)
    : cpu(cpu), ppu(ppu), frameDone(false)
{
}

void Emulator::step() {
    int cpuClocks = cpu.tickCycle();

    for (int i = 0; i < cpuClocks * 3; ++i) {
        ppu.stepDot();

        if (ppu.isNmiTriggered()) {
            cpu.requestNmi();
            ppu.clearNmiFlag();
        }

        if (ppu.getScanline() == 261 && ppu.getCycle() == 1) {
            frameDone = true;
            break;
        }
    }
}

CPU* Emulator::getCPU() const {
    return &cpu;
}

PPU* Emulator::getPPU() const {
    return &ppu;
}

bool Emulator::frameComplete() const {
    return frameDone;
}

void Emulator::resetFrameFlag() {
    frameDone = false;
}

const uint8_t* Emulator::getFrameBuffer() const {
    return ppu.getFrameBuffer();
}