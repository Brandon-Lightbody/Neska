#include "mapper.h"
#include <iostream>

void Mapper::init(uint8_t prgBanks, uint8_t chrBanks,
    const std::vector<uint8_t>& prgData,
    const std::vector<uint8_t>& chrData) { }
uint8_t Mapper::cpuRead(uint16_t addr) { return 0; }
void Mapper::cpuWrite(uint16_t addr, uint8_t value) { }
uint8_t Mapper::ppuRead(uint16_t addr) { return 0; }
void Mapper::ppuWrite(uint16_t addr, uint8_t value) { }

std::unique_ptr<Mapper> createMapper(uint8_t mapperID)
{
    switch (mapperID) {
    case 0:  return std::make_unique<NROM>();
    case 1:  return std::make_unique<MMC1>();
    case 2:  return std::make_unique<UNROM>();
    case 3:  return std::make_unique<CNROM>();
    default:
        std::cerr << "Unsupported mapper " << int(mapperID)
            << "; defaulting to NROM\n";
        return std::make_unique<NROM>();
    }
}

void NROM::init(uint8_t prgBanks, uint8_t chrBanks,
    const std::vector<uint8_t>& prgData,
    const std::vector<uint8_t>& chrData)
{
    prgBanksCount = prgBanks;
    chrBanksCount = chrBanks;

    // Load PRG ROM and allocate PRG RAM (8 KB)
    prgROM = prgData;
    prgRAM.assign(0x2000, 0);

    // CHR ROM or RAM
    if (chrBanksCount == 0) {
        hasChrRam = true;
        chrRam.assign(0x2000, 0);
    }
    else {
        hasChrRam = false;
        chrRom = chrData;
    }
}

uint8_t NROM::cpuRead(uint16_t addr) {
    // Cartridge RAM ($6000–$7FFF)
    if (addr >= 0x6000 && addr < 0x8000) {
        return prgRAM[addr - 0x6000];
    }
    // PRG ROM ($8000–$FFFF)
    if (addr >= 0x8000) {
        uint32_t index;
        if (prgBanksCount == 1) {
            // 16 KB mirrored
            index = addr & 0x3FFF;
        }
        else {
            // Two 16 KB banks: switch at 0xC000
            index = (addr < 0xC000)
                ? (addr & 0x3FFF)
                : (0x4000 | (addr & 0x3FFF));
        }
        return prgROM[index];
    }
    return 0;
}

void NROM::cpuWrite(uint16_t addr, uint8_t value) {
    if (addr >= 0x6000 && addr < 0x8000) {
        prgRAM[addr - 0x6000] = value;
    }
}

uint8_t NROM::ppuRead(uint16_t addr) {
    // CHR ($0000–$1FFF)
    addr &= 0x1FFF;
    if (hasChrRam) {
        return chrRam[addr];
    }
    return chrRom[addr];
}

void NROM::ppuWrite(uint16_t addr, uint8_t value) {
    if (hasChrRam) {
        chrRam[addr & 0x1FFF] = value;
    }
}

void MMC1::init(uint8_t pB, uint8_t cB,
    const std::vector<uint8_t>& pData,
    const std::vector<uint8_t>& cData)
{
    prgBanks = pB;
    chrBanks = cB;
    prgROM = pData;

    if (chrBanks == 0) {                 // CHR‑RAM (8 KB)
        hasChrRAM = true;
        chrRAM.assign(0x2000, 0);
    }
    else {
        chrROM = cData;
    }
}

uint8_t MMC1::cpuRead(uint16_t addr)
{
    if (addr < 0x6000) return 0;                     // open bus / trainer

    if (addr >= 0x6000 && addr < 0x8000) {           // optional PRG‑RAM
        return 0; // not implemented
    }

    // PRG‑ROM $8000‑FFFF
    const bool prg16 = (control & 0x08) != 0;        // 0 = 32 KB, 1 = 16 KB
    const bool fixHigh = (control & 0x0C) == 0x0C;   // mode 3
    const bool fixLow = (control & 0x0C) == 0x08;   // mode 2

    uint32_t bank;
    if (!prg16) {                                    // 32 KB switch
        bank = (prgBank & 0x0E) * 0x4000;
        return prgROM[bank + (addr & 0x7FFF)];
    }

    if (fixLow && addr < 0xC000) {                   // $8000‑BFFF fixed to bank 0
        bank = 0;
    }
    else if (fixHigh && addr >= 0xC000) {          // $C000‑FFFF fixed to last
        bank = (prgBanks - 1) * 0x4000;
    }
    else {                                         // switchable
        bank = prgBank * 0x4000;
    }
    return prgROM[bank + (addr & 0x3FFF)];
}

void MMC1::cpuWrite(uint16_t addr, uint8_t value)
{
    if (addr < 0x8000) return;                       // ignore RAM writes here

    if (value & 0x80) {                              // reset shift register
        shiftReg = 0x10;
        control |= 0x0C;                             // force 16 KB mode
        return;
    }

    // shift one in (LSB‑first)
    bool complete = shiftReg & 1;
    shiftReg >>= 1;
    shiftReg |= (value & 1) << 4;

    if (complete) {                                  // 5 writes collected
        commitRegister(addr);
        shiftReg = 0x10;
    }
}

uint8_t MMC1::ppuRead(uint16_t addr)
{
    addr &= 0x1FFF;
    size_t bankOffset;

    if (hasChrRAM) return chrRAM[addr];

    const bool chr4 = (control & 0x10) != 0;         // 0 = 8 KB, 1 = 4 KB
    if (!chr4) {
        bankOffset = (chrBank0 & 0x1E) * 0x2000;
        return chrROM[bankOffset + addr];
    }
    if (addr < 0x1000)
        bankOffset = chrBank0 * 0x1000;
    else
        bankOffset = chrBank1 * 0x1000;

    return chrROM[bankOffset + (addr & 0x0FFF)];
}

void MMC1::ppuWrite(uint16_t addr, uint8_t value)
{
    if (hasChrRAM) chrRAM[addr & 0x1FFF] = value;
}

void MMC1::commitRegister(uint16_t addr)
{
    uint8_t data = shiftReg & 0x1F;
    switch ((addr >> 13) & 0x03) { // $8000, $A000, $C000, $E000
    case 0: control = data; break;
    case 1: chrBank0 = data; break;
    case 2: chrBank1 = data; break;
    case 3: prgBank = data & 0x0F; break;
    }
}

void UNROM::init(uint8_t pB, uint8_t /*cB*/,
    const std::vector<uint8_t>& pData,
    const std::vector<uint8_t>& /*cData*/)
{
    prgBanks = pB;
    prgROM = pData;
    chrRAM.assign(0x2000, 0);          // UNROM always uses CHR‑RAM
}

uint8_t UNROM::cpuRead(uint16_t addr)
{
    if (addr < 0x8000) return 0;                       // no PRG‑RAM here
    uint32_t index;
    if (addr < 0xC000) {                               // switchable 16 KB
        index = bankSelect * 0x4000 + (addr & 0x3FFF);
    }
    else {                                           // fixed to last bank
        index = (prgBanks - 1) * 0x4000 + (addr & 0x3FFF);
    }
    return prgROM[index];
}

void UNROM::cpuWrite(uint16_t addr, uint8_t value)
{
    if (addr >= 0x8000) bankSelect = value & 0x0F;
}

uint8_t UNROM::ppuRead(uint16_t addr)
{
    return chrRAM[addr & 0x1FFF];
}

void UNROM::ppuWrite(uint16_t addr, uint8_t value)
{
    chrRAM[addr & 0x1FFF] = value;
}

void CNROM::init(uint8_t pB, uint8_t cB,
    const std::vector<uint8_t>& pData,
    const std::vector<uint8_t>& cData)
{
    prgBanks = pB;
    chrBanks = cB;
    prgROM = pData;
    chrROM = cData;
}

uint8_t CNROM::cpuRead(uint16_t addr)
{
    if (addr < 0x8000) return 0;
    uint32_t index;
    if (prgBanks == 1)
        index = addr & 0x3FFF;                // 16 KB mirror
    else
        index = addr & 0x7FFF;                // full 32 KB
    return prgROM[index];
}

void CNROM::cpuWrite(uint16_t addr, uint8_t value)
{
    if (addr >= 0x8000) chrBankSelect = value & 0x03;   // 2‑bit select
}

uint8_t CNROM::ppuRead(uint16_t addr)
{
    addr &= 0x1FFF;
    uint32_t index = chrBankSelect * 0x2000 + addr;
    return chrROM[index];
}

void CNROM::ppuWrite(uint16_t /*addr*/, uint8_t /*value*/)
{
    /* CHR‑ROM – writes ignored */
}