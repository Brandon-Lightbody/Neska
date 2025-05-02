// mapper.cpp
#include "mapper.h"
#include <iostream>

//-----------------------------------------------------------------------------
// Base Mapper: store raw PRG/CHR banks
//-----------------------------------------------------------------------------

void Mapper::init(uint8_t prgBanks, uint8_t chrBanks,
    const std::vector<uint8_t>& prg,
    const std::vector<uint8_t>& chr)
{
    prgROM = prg;
    prgRAM.assign(0x2000, 0);    // 8 KB battery-backed RAM
    chrROM = chr;
    hasChrRam = (chrBanks == 0);
    prgBanksCount = prgBanks;
    chrBanksCount = chrBanks;
}

//-----------------------------------------------------------------------------
// Mapper 0 (NROM): no bank-switching, just mirror or direct index
//-----------------------------------------------------------------------------

class Mapper0 : public Mapper {
public:
    // CPU-side: PRG-RAM ($6000–$7FFF) or PRG-ROM ($8000–$FFFF)
    uint8_t cpuRead(uint16_t addr) override {
        if (addr >= 0x6000 && addr < 0x8000)
            return prgRAM[addr - 0x6000];

        if (addr >= 0x8000) {
            size_t idx = (addr - 0x8000) % prgROM.size();
            return prgROM[idx];
        }

        // everything else is caught in Memory
        return 0x00;
    }

    void cpuWrite(uint16_t addr, uint8_t data) override {
        if (addr >= 0x6000 && addr < 0x8000)
            prgRAM[addr - 0x6000] = data;
        // writes to PRG-ROM are ignored
    }

    // PPU-side: CHR-ROM ($0000–$1FFF), or CHR-RAM if present
    uint8_t ppuRead(uint16_t addr) override {
        return chrROM[addr & 0x1FFF];
    }

    void ppuWrite(uint16_t addr, uint8_t data) override {
        if (hasChrRam)
            chrROM[addr & 0x1FFF] = data;
    }
};

//-----------------------------------------------------------------------------
// Factory
//-----------------------------------------------------------------------------

std::unique_ptr<Mapper> createMapper(uint8_t mapperID) {
    switch (mapperID) {
    case 0:  return std::make_unique<Mapper0>();
    default:
        std::cerr << "Unsupported mapper " << int(mapperID)
            << " – defaulting to NROM.\n";
        return std::make_unique<Mapper0>();
    }
}
