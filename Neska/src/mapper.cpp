#include "mapper.h"
#include <iostream>
#include <algorithm>

// Factory: choose appropriate mapper by ID
std::unique_ptr<Mapper> createMapper(uint8_t mapperID) {
    switch (mapperID) {
    case 0:  return std::make_unique<Mapper0>();
    case 1:  return std::make_unique<Mapper1>();
    case 2:  return std::make_unique<Mapper2>();
    case 3:  return std::make_unique<Mapper3>();
    default:
        std::cerr << "Mapper #" << int(mapperID)
            << " not implemented, falling back to NROM (Mapper0).\n";
        return std::make_unique<Mapper0>();
    }
}

// ===========================
// Mapper0: NROM
// ===========================
void Mapper0::initMapper(uint8_t prgBanks,
    uint8_t chrBanks,
    const std::vector<uint8_t>& prgData,
    const std::vector<uint8_t>& chrData) {
    prgBanksCount = prgBanks;
    chrBanksCount = chrBanks;
    prgROM = prgData;
    prgRAM.assign(0x2000, 0);
    hasChrRam = (chrBanks == 0);
    if (chrBanks == 0) {
        chrROM.assign(0x2000, 0);
    }
    else {
        chrROM = chrData;
    }
}

uint8_t Mapper0::cpuRead(uint16_t addr) {
    if (addr >= 0x6000 && addr < 0x8000) {
        return prgRAM[addr - 0x6000];
    }
    uint32_t offset = addr - 0x8000;
    if (prgBanksCount == 1) offset &= 0x3FFF;
    return prgROM[offset];
}

void Mapper0::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x6000 && addr < 0x8000) {
        prgRAM[addr - 0x6000] = data;
    }
}

uint8_t Mapper0::ppuRead(uint16_t addr) {
    uint8_t v = chrROM[addr & 0x1FFF];
    return v;
}

void Mapper0::ppuWrite(uint16_t addr, uint8_t data) {
    if (!hasChrRam) return;
    chrROM[addr & 0x1FFF] = data;
}

// ===========================
// Mapper1: MMC1
// ===========================
void Mapper1::initMapper(uint8_t prgBanks,
    uint8_t chrBanks,
    const std::vector<uint8_t>& prgData,
    const std::vector<uint8_t>& chrData) {
    prgBanksCount = prgBanks;
    chrBanksCount = chrBanks;
    prgROM = prgData;
    prgRAM.assign(0x2000, 0);
    hasChrRam = (chrBanks == 0);
    if (chrBanks == 0) {
        chrROM.assign(0x2000, 0);
    }
    else {
        chrROM = chrData;
    }
    shiftReg = 0; shiftCount = 0;
    control = 0x0C; prgMode = true; chrMode = false;
    prgBank = prgBanks - 1; chrBank0 = chrBank1 = 0;
}

uint8_t Mapper1::cpuRead(uint16_t addr) {
    if (addr >= 0x6000 && addr < 0x8000) {
        return prgRAM[addr - 0x6000];
    }
    if (addr < 0x8000) {
        return 0;  // open bus
    }
    return prgROM[getPRGAddress(addr)];
}

void Mapper1::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x6000 && addr < 0x8000) {
        prgRAM[addr - 0x6000] = data;
        return;
    }
    if (addr < 0x8000) return;

    if (data & 0x80) {
        shiftReg = 0; shiftCount = 0;
        control |= 0x0C;
        return;
    }
    shiftReg |= (data & 1) << shiftCount;
    shiftCount++;
    if (shiftCount == 5) {
        uint8_t reg = (addr >> 13) & 0x03;
        switch (reg) {
        case 0:
            control = shiftReg & 0x1F;
            chrMode = (control & 0x10) != 0;
            prgMode = (control & 0x08) != 0;
            break;
        case 1: chrBank0 = shiftReg & 0x1F; break;
        case 2: chrBank1 = shiftReg & 0x1F; break;
        case 3: prgBank = shiftReg & 0x0F; break;
        }
        shiftReg = 0; shiftCount = 0;
    }
}

uint8_t Mapper1::ppuRead(uint16_t addr) {
    if (addr >= 0x2000) return 0;
    return chrROM[getCHRAddress(addr)];
}

void Mapper1::ppuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x2000 || !hasChrRam) return;
    chrROM[getCHRAddress(addr)] = data;
}

uint32_t Mapper1::getPRGAddress(uint16_t addr) const {
    if (!prgMode) {
        uint8_t bank = prgBank >> 1;
        return bank * 0x8000 + (addr - 0x8000);
    }
    if (addr < 0xC000) {
        return prgBank * 0x4000 + (addr - 0x8000);
    }
    else {
        return (prgBanksCount - 1) * 0x4000 + (addr - 0xC000);
    }
}

uint32_t Mapper1::getCHRAddress(uint16_t addr) const {
    if (!chrMode) {
        return (chrBank0 >> 1) * 0x2000 + (addr & 0x1FFF);
    }
    if (addr < 0x1000) {
        return chrBank0 * 0x1000 + (addr & 0x0FFF);
    }
    else {
        return chrBank1 * 0x1000 + (addr & 0x0FFF);
    }
}

// ===========================
// Mapper2: UxROM
// ===========================
void Mapper2::initMapper(uint8_t prgBanks,
    uint8_t chrBanks,
    const std::vector<uint8_t>& prgData,
    const std::vector<uint8_t>& chrData) {
    prgBanksCount = prgBanks;
    chrBanksCount = chrBanks;
    prgROM = prgData;
    prgRAM.assign(0x2000, 0);
    hasChrRam = (chrBanks == 0);
    if (chrBanks == 0) {
        chrROM.assign(0x2000, 0);
    }
    else {
        chrROM = chrData;
    }
    bankSelect = 0;
}

uint8_t Mapper2::cpuRead(uint16_t addr) {
    if (addr >= 0x6000 && addr < 0x8000) {
        return prgRAM[addr - 0x6000];
    }
    if (addr < 0x8000) return 0;
    if (addr < 0xC000) {
        return prgROM[bankSelect * 0x4000 + (addr - 0x8000)];
    }
    else {
        return prgROM[(prgBanksCount - 1) * 0x4000 + (addr - 0xC000)];
    }
}

void Mapper2::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x6000 && addr < 0x8000) {
        prgRAM[addr - 0x6000] = data;
        return;
    }
    if (addr >= 0x8000) {
        bankSelect = data & 0x0F;
    }
}

uint8_t Mapper2::ppuRead(uint16_t addr) {
    if (addr >= 0x2000) return 0;
    return chrROM[addr & 0x1FFF];
}

void Mapper2::ppuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x2000 || !hasChrRam) return;
    chrROM[addr & 0x1FFF] = data;
}

// ===========================
// Mapper3: CNROM
// ===========================
void Mapper3::initMapper(uint8_t prgBanks,
    uint8_t chrBanks,
    const std::vector<uint8_t>& prgData,
    const std::vector<uint8_t>& chrData) {
    prgBanksCount = prgBanks;
    chrBanksCount = chrBanks;
    prgROM = prgData;
    prgRAM.assign(0x2000, 0);
    hasChrRam = (chrBanks == 0);
    if (chrBanks == 0) {
        chrROM.assign(0x2000, 0);
    }
    else {
        chrROM = chrData;
    }
    chrBankSelect = 0;
}

uint8_t Mapper3::cpuRead(uint16_t addr) {
    if (addr >= 0x6000 && addr < 0x8000) {
        return prgRAM[addr - 0x6000];
    }
    if (addr < 0x8000) return 0;
    uint32_t off = addr - 0x8000;
    if (prgBanksCount == 1) off &= 0x3FFF;
    return prgROM[off];
}

void Mapper3::cpuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x6000 && addr < 0x8000) {
        prgRAM[addr - 0x6000] = data;
        return;
    }
    if (addr >= 0x8000) {
        chrBankSelect = data & 0x03;
    }
}

uint8_t Mapper3::ppuRead(uint16_t addr) {
    if (addr >= 0x2000) return 0;
    return chrROM[chrBankSelect * 0x2000 + (addr & 0x1FFF)];
}

void Mapper3::ppuWrite(uint16_t addr, uint8_t data) {
    if (addr >= 0x2000 || !hasChrRam) return;
    chrROM[chrBankSelect * 0x2000 + (addr & 0x1FFF)] = data;
}
