#pragma once

#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include "core.h"
#include "mapper.h"

class CPU;
class PPU;

class MemoryBus {
public:
    MemoryBus();

    void connectCPU(CPU* cpu);
    void connectPPU(PPU* ppu);

    MirrorMode loadROM(const std::string& path);

    uint8_t cpuRead(uint16_t addr);
    void    cpuWrite(uint16_t addr, uint8_t val);
    uint8_t cpuPeek(uint16_t addr) const;  // for debugger / open-bus behavior

    uint8_t ppuRead(uint16_t addr) const;
    void    ppuWrite(uint16_t addr, uint8_t val);
    uint8_t ppuPeek(uint16_t addr) const;

    void setButtonPressed(int bit);
    void clearButtonPressed(int bit);
private:
    std::array<uint8_t, 0x0800> ram;
    std::array<uint8_t, 0x2000> prgRam;

    std::array<uint8_t, 0x1000> nametables;
    std::array<uint8_t, 0x20> palette;
    MirrorMode mirrorMode;

    bool    strobe;
    uint8_t controllerState;
    uint8_t controllerShift;

    PPU* ppu;
    CPU* cpu;

    std::unique_ptr<Mapper> mapper;

    uint8_t readController();
    uint8_t readSecondController();
    void    strobeController(uint8_t val);
    void    runOamDma(uint8_t page);
    uint8_t openBus() const;
};