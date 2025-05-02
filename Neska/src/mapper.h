#pragma once

#include <cstdint>
#include <vector>
#include <memory>

class PPU;

// Abstract base class for all mappers
class Mapper {
public:
    virtual ~Mapper() = default;

    // Initialize with PRG and CHR data
    virtual void init(uint8_t prgBanks, uint8_t chrBanks, const std::vector<uint8_t>& prg, const std::vector<uint8_t>& chr);

    // CPU read/write ($6000-$FFFF)
    virtual uint8_t cpuRead(uint16_t addr) = 0;
    virtual void    cpuWrite(uint16_t addr, uint8_t data) = 0;

    // PPU read/write ($0000-$1FFF)
    virtual uint8_t ppuRead(uint16_t addr) = 0;
    virtual void    ppuWrite(uint16_t addr, uint8_t data) = 0;

protected:
    PPU* ppu;

    std::vector<uint8_t> prgROM;
    std::vector<uint8_t> prgRAM;
    std::vector<uint8_t> chrROM;

    bool hasChrRam = false;
    uint8_t prgBanksCount = 0;
    uint8_t chrBanksCount = 0;
};

// Factory function
std::unique_ptr<Mapper> createMapper(uint8_t mapperID);