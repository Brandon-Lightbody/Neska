// memory.h
#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include "core.h"
#include "mapper.h"

// forward
class CPU;
class PPU;

class Memory {
public:
    Memory();

    // Must be called immediately after constructing PPU/CPU
    void setPPU(PPU* p);
    void setCPU(CPU* p);

    // Load an iNES file, initialize the mapper, and return the mirroring mode.
    // chrRomOut will be filled with CHR data (or CHR‑RAM contents).
    MirrorMode loadROM(const std::string& path, std::vector<uint8_t>& chrRomOut);

    // CPU‐side bus access
    uint8_t read(uint16_t addr);
    void    write(uint16_t addr, uint8_t val);

    // PPU‐side bus access
    uint8_t ppuRead(uint16_t addr)  const;
    void    ppuWrite(uint16_t addr, uint8_t val) const;

    void setButtonPressed(int bit);
    void clearButtonPressed(int bit);
private:
    // 2 KB internal RAM
    std::vector<uint8_t> ram;

    // Controller strobe & shift register
    bool    strobe;
    uint8_t controllerState;
    uint8_t controllerShift;

    // PPU & CPU pointers for I/O
    PPU* ppu;
    CPU* cpu;

    // Cartridge logic
    std::unique_ptr<Mapper> mapper;

    // Helpers
    uint8_t readController();
    uint8_t readSecondController();
    void    strobeController(uint8_t val);
    void    runOamDma(uint8_t page);
    uint8_t openBus() const;
};
