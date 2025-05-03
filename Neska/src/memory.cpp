// memory.cpp
#include "memory.h"
#include "cpu.h"
#include "ppu.h"
#include "mapper.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

Memory::Memory()
    : ram(0x0800, 0),
    strobe(false),
    controllerState(0),
    controllerShift(0),
    ppu(nullptr),
    cpu(nullptr)
{
}

void Memory::setPPU(PPU* p) {
    ppu = p;
}

void Memory::setCPU(CPU* c) {
    cpu = c;
}

MirrorMode Memory::loadROM(const std::string& filename, std::vector<uint8_t>& chrRomOut) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Unable to open ROM: " << filename << "\n";
        return MirrorMode::HORIZONTAL;
    }

    // Read entire file into buffer
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();

    if (fileSize < 16) {
        std::cerr << "File too small for iNES header.\n";
        return MirrorMode::HORIZONTAL;
    }

    // Validate iNES signature "NES" 0x1A
    if (!(buffer[0] == 'N' && buffer[1] == 'E' && buffer[2] == 'S' && buffer[3] == 0x1A)) {
        std::cerr << "Not a valid iNES ROM.\n";
        return MirrorMode::HORIZONTAL;
    }

    bool isINES2 = ((buffer[7] & 0x0C) == 0x08);

    uint8_t prgBanks = buffer[4];
    uint8_t chrBanks = buffer[5];
    uint8_t flags6 = buffer[6];
    uint8_t flags7 = buffer[7];

    bool hasTrainer = (flags6 & 0x04) != 0;
    bool fourScreen = (flags6 & 0x08) != 0;

    MirrorMode mirror;
    if (fourScreen) {
        mirror = MirrorMode::FOUR_SCREEN;
    }
    else {
        bool vert = (flags6 & 0x01) != 0;
        mirror = vert ? MirrorMode::VERTICAL : MirrorMode::HORIZONTAL;
    }

    uint8_t mapperLow = (flags6 >> 4) & 0x0F;
    uint8_t mapperHigh = flags7 & 0xF0;
    uint8_t mapperID = mapperHigh | mapperLow;

    if (isINES2) {
        uint8_t mapperExt = buffer[8] & 0x0F;
        mapperID |= mapperExt << 4;
    }

    const size_t headerSize = 16;
    size_t trainerSize = hasTrainer ? 512 : 0;
    size_t prgSize = size_t(prgBanks) * 0x4000; // 16KB each
    size_t chrSize = size_t(chrBanks) * 0x2000; //  8KB each
    size_t prgOffset = headerSize + trainerSize;
    size_t chrOffset = prgOffset + prgSize;

    if (buffer.size() < prgOffset + prgSize + chrSize) {
        std::cerr << "ROM file seems truncated.\n";
        // still initialize what we can
    }

    // Extract PRG data
    std::vector<uint8_t> prgData(prgSize);
    std::copy_n(buffer.begin() + prgOffset, prgSize, prgData.begin());

    // Extract CHR data or allocate CHR RAM
    std::vector<uint8_t> chrData;
    if (chrSize == 0) {
        chrData.resize(0x2000, 0);
    }
    else {
        chrData.resize(chrSize);
        std::copy_n(buffer.begin() + chrOffset, chrSize, chrData.begin());
    }

    // Initialize mapper
    mapper = createMapper(mapperID);
    mapper->initMapper(prgBanks, chrBanks, prgData, chrData);

    // Return CHR contents for PPU
    chrRomOut = std::move(chrData);

    std::cout << "Loaded ROM: PRG=" << int(prgBanks)
        << "×16KB, CHR=" << int(chrBanks)
        << "×8KB, Mapper=" << int(mapperID)
        << ", Mirror=" << (mirror == MirrorMode::VERTICAL ? "Vertical" : "Horizontal")
        << "\n";

    return mirror;
}

uint8_t Memory::read(uint16_t addr) {
    // 2 KB internal RAM, mirrored every 0x800
    if (addr < 0x2000) {
        return ram[addr & 0x07FF];
    }
    // PPU registers, mirrored every 8 bytes
    if (addr < 0x4000) {
        return ppu->readRegister(0x2000 | (addr & 0x0007));
    }
    // APU & I/O
    if (addr < 0x4020) {
        switch (addr) {
        case 0x4016: return readController();
        case 0x4017: return readSecondController();
        default:     return openBus();
        }
    }
    // Cartridge (PRG-ROM/RAM, bank switching)
    return mapper->cpuRead(addr);
}

void Memory::write(uint16_t addr, uint8_t val) {
    // 2 KB internal RAM
    if (addr < 0x2000) {
        ram[addr & 0x07FF] = val;
        return;
    }
    // PPU registers
    if (addr < 0x4000) {
        ppu->writeRegister(0x2000 | (addr & 0x0007), val);
        return;
    }
    // APU & I/O
    if (addr < 0x4020) {
        switch (addr) {
        case 0x4014:
            runOamDma(val);
            break;
        case 0x4016:
            strobeController(val);
            break;
        default:
            // Other APU or I/O writes…
            break;
        }
        return;
    }
    // Cartridge
    mapper->cpuWrite(addr, val);
}

uint8_t Memory::ppuRead(uint16_t addr) const {
    if (addr < 0x2000) {
        // CHR → straight to mapper
        return mapper->ppuRead(addr);
    }
    // Nametables & palette → let the PPU handle mirroring + buffer
    return ppu->vramRead(addr);
}

void Memory::ppuWrite(uint16_t addr, uint8_t val) const {
    if (addr < 0x2000) {
        mapper->ppuWrite(addr, val);
    }
    else {
        ppu->vramWrite(addr, val);
    }
}

uint8_t Memory::readController() {
    uint8_t bit = controllerShift & 1;
    controllerShift >>= 1;
    // upper bits open bus
    return bit | 0x40;
}

uint8_t Memory::readSecondController() {
    // No second controller implemented; open bus upper bits
    return 0x40;
}

void Memory::strobeController(uint8_t val) {
    bool newStrobe = (val & 1) != 0;
    if (newStrobe) {
        controllerShift = controllerState;
    }
    strobe = newStrobe;
}

void Memory::runOamDma(uint8_t page) {
    if (!ppu || !cpu) return;
    uint16_t base = uint16_t(page) << 8;
    for (int i = 0; i < 256; i++) {
        uint8_t data = read(base + i);
        ppu->writeOAM(data);
    }
    cpu->stallCycles = 513;  // 512 or 513 on odd cycles
}

uint8_t Memory::openBus() const {
    // Return a neutral open-bus value
    return 0;
}

void Memory::setButtonPressed(int bit) {
    if (bit >= 0 && bit < 8)
        controllerState |= (1 << bit);
}

void Memory::clearButtonPressed(int bit) {
    if (bit >= 0 && bit < 8)
        controllerState &= ~(1 << bit);
}