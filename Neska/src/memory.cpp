// memory.cpp
#include "memory.h"
#include "ppu.h"
#include "cpu.h"
#include "core.h"
#include "mapper.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

//-----------------------------------------------------------------------------
// Constructor / Wiring
//-----------------------------------------------------------------------------

Memory::Memory()
    : ram(0x800)
    , strobe(false)
    , controllerState(0)
    , controllerShift(0)
    , ppu(nullptr)
    , cpu(nullptr)
    , mapper(nullptr)
{
}

void Memory::setPPU(PPU* p) {
    ppu = p;
}

void Memory::setCPU(CPU* p) {
    cpu = p;
}

//-----------------------------------------------------------------------------
// loadROM: parse iNES header, extract PRG/CHR, instantiate mapper
//-----------------------------------------------------------------------------

MirrorMode Memory::loadROM(const std::string& path, std::vector<uint8_t>& chrRomOut) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("Failed to open ROM: " + path);

    // 1) Read header
    uint8_t header[16];
    ifs.read(reinterpret_cast<char*>(header), 16);
    if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A)
        throw std::runtime_error("Invalid iNES header in " + path);

    uint8_t prgBanks = header[4];
    uint8_t chrBanks = header[5];
    uint8_t flags6 = header[6];
    uint8_t flags7 = header[7];

    // 2) Determine mirroring and mapper ID
    MirrorMode mirror = (flags6 & 0x01) ? MirrorMode::VERTICAL : MirrorMode::HORIZONTAL;
    uint8_t mapperID = (flags6 >> 4) | (flags7 & 0xF0);

    // 3) Skip trainer if present
    if (flags6 & 0x04) {
        ifs.seekg(512, std::ios::cur);
    }

    // 4) Read PRG data (16 KB per bank)
    std::vector<uint8_t> prgData(prgBanks * 0x4000);
    ifs.read(reinterpret_cast<char*>(prgData.data()), prgData.size());

    // 5) Read CHR data (8 KB per bank, or allocate RAM if zero)
    std::vector<uint8_t> chrData((chrBanks ? chrBanks * 0x2000 : 0x2000));
    ifs.read(reinterpret_cast<char*>(chrData.data()), chrData.size());

    // 6) Instantiate mapper and hand off banks
    mapper = createMapper(mapperID);
    mapper->init(prgBanks, chrBanks, prgData, chrData);

    // 7) Give PPU its CHR buffer
    chrRomOut = std::move(chrData);
    return mirror;
}

//-----------------------------------------------------------------------------
// CPU-side bus: read / write
//-----------------------------------------------------------------------------

uint8_t Memory::read(uint16_t addr) {
    // 1) 2 KiB internal RAM, mirrored every 0x800
    if (addr < 0x2000) {
        return ram[addr & 0x07FF];
    }

    // 2) PPU registers $2000–$2007 mirrored to $3FFF
    if (addr < 0x4000) {
        return ppu->readRegister(0x2000 + (addr & 0x7));
    }

    // 3) OAM DMA register ($4014) is write-only → open bus
    if (addr == 0x4014) {
        return openBus();
    }

    // 4) Controllers / APU ($4016–$4017)
    if (addr == 0x4016) return readController();
    if (addr == 0x4017) return readSecondController();

    // 5) APU / I/O / expansion ($4000–$5FFF)
    if (addr < 0x6000) {
        return openBus();
    }

    // 6) Cartridge space: PRG-RAM or PRG-ROM
    return mapper->cpuRead(addr);
}

void Memory::write(uint16_t addr, uint8_t val) {
    // 1) 2 KiB internal RAM
    if (addr < 0x2000) {
        ram[addr & 0x07FF] = val;
        return;
    }

    // 2) PPU registers $2000–$2007
    if (addr < 0x4000) {
        ppu->writeRegister(0x2000 + (addr & 0x7), val);
        return;
    }

    // 3) OAM DMA trigger
    if (addr == 0x4014) {
        runOamDma(val);
        return;
    }

    // 4) Controllers / APU strobe ($4016–$4017)
    if (addr == 0x4016) { strobeController(val); return; }
    if (addr == 0x4017) { /* could hook 2nd controller or APU */ return; }

    // 5) APU / I/O / expansion
    if (addr < 0x6000) {
        return;
    }

    // 6) Cartridge space: PRG-RAM or PRG-ROM
    mapper->cpuWrite(addr, val);
}

uint8_t Memory::peek(uint16_t addr) const {
    // Unchecked read for debugger
    if (addr < 0x2000)        return ram[addr & 0x07FF];
    if (addr < 0x4000)        return ppu->peekRegister(addr & 0x7);
    if (addr >= 0x6000)       return mapper->cpuRead(addr);
    return 0;
}

void Memory::writeToMapper(uint16_t addr, uint8_t val) {
    mapper->ppuWrite(addr, val);
}

uint8_t Memory::readFromMapper(uint16_t addr) {
    return mapper->ppuRead(addr);
}

//-----------------------------------------------------------------------------
// PPU-side pass-through
//-----------------------------------------------------------------------------

uint8_t Memory::ppuBusRead(uint16_t addr) const {
    return ppu->busRead(addr);
}

void Memory::ppuBusWrite(uint16_t addr, uint8_t val) const {
    ppu->busWrite(addr, val);
}

//-----------------------------------------------------------------------------
// Controller support (strobe / shift register)
//-----------------------------------------------------------------------------

void Memory::setButtonPressed(int bit) {
    controllerState |= (1 << bit);
}

void Memory::clearButtonPressed(int bit) {
    controllerState &= ~(1 << bit);
}

uint8_t Memory::readController() {
    // Return current bit, then advance if not strobed
    uint8_t ret = (controllerState >> controllerShift) & 1;
    if (!strobe) {
        controllerShift = std::min(controllerShift + 1, 7);
    }
    return ret;
}

uint8_t Memory::readSecondController() {
    // Stubbed: no 2nd controller
    return 0;
}

void Memory::strobeController(uint8_t val) {
    strobe = val & 1;
    if (strobe) {
        controllerShift = 0;
    }
}

//-----------------------------------------------------------------------------
// OAM DMA: copy 256 bytes from CPU page to PPU OAM
//-----------------------------------------------------------------------------

void Memory::runOamDma(uint8_t page) {
    uint16_t base = uint16_t(page) << 8;
    for (int i = 0; i < 256; ++i) {
        uint8_t b = read(base + i);
        ppu->writeOAM(b);
    }
}

//-----------------------------------------------------------------------------
// Open-bus approximation
//-----------------------------------------------------------------------------

uint8_t Memory::openBus() const {
    // Typically returns last value on the bus; stub 0
    return 0;
}
