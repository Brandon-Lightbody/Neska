#include "memory_bus.h"
#include "ppu.h"
#include "cpu.h"
#include "core.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

uint16_t mirrorAddress(uint16_t addr, MirrorMode mirrorMode);

MemoryBus::MemoryBus()
    : strobe(false), controllerState(0), controllerShift(0),
    ppu(nullptr), cpu(nullptr), mapper(nullptr)
{
    mirrorMode = MirrorMode::HORIZONTAL;
    std::memset(ram.data(), 0, ram.size());
    std::memset(prgRam.data(), 0, prgRam.size());
    std::memset(nametables.data(), 0, nametables.size());
    std::memset(palette.data(), 0, palette.size());
}

void MemoryBus::connectPPU(PPU* p) {
    ppu = p;
}

void MemoryBus::connectCPU(CPU* p) {
    cpu = p;
}

//-----------------------------------------------------------------------------
// loadROM: parse iNES header, extract PRG/CHR, instantiate mapper
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// loadROM: parse iNES header, set up mirroring + mapper
//-----------------------------------------------------------------------------
MirrorMode MemoryBus::loadROM(const std::string& path)
{
    std::ifstream rom(path, std::ios::binary);
    if (!rom) throw std::runtime_error("Could not open ROM: " + path);

    uint8_t hdr[16];
    rom.read(reinterpret_cast<char*>(hdr), 16);
    if (std::memcmp(hdr, "NES\x1A", 4) != 0)
        throw std::runtime_error("Bad iNES header");

    const uint8_t prgBanks = hdr[4];
    const uint8_t chrBanks = hdr[5];
    const uint8_t flags6 = hdr[6];
    const uint8_t flags7 = hdr[7];

    /* -----------------------------------------------------------
       Mirroring priority:
       bit 3 (four‑screen) overrides bit 0 (H/V).  If both are 0
       the mapper is expected to provide single‑screen control.
    ----------------------------------------------------------- */
    if (flags6 & 0x08)            mirrorMode = MirrorMode::FOUR_SCREEN;
    else if (flags6 & 0x01)       mirrorMode = MirrorMode::VERTICAL;
    else                          mirrorMode = MirrorMode::HORIZONTAL;

    const uint8_t mapperID = (flags6 >> 4) | (flags7 & 0xF0);

    if (flags6 & 0x04) rom.seekg(512, std::ios::cur);   // optional trainer

    /* ------- PRG & CHR transfer into mapper ------------------- */
    std::vector<uint8_t> prg(prgBanks * 0x4000);
    rom.read(reinterpret_cast<char*>(prg.data()), prg.size());

    std::vector<uint8_t> chr((chrBanks ? chrBanks : 1) * 0x2000);
    rom.read(reinterpret_cast<char*>(chr.data()), chr.size());

    mapper = createMapper(mapperID);
    mapper->init(prgBanks, chrBanks, prg, chr);

    /* ------- clear nametable RAM (full 4 KiB) ------------------ */
    std::fill(nametables.begin(), nametables.end(), 0);

    return mirrorMode;        // PPU gets this immediately
}

uint8_t MemoryBus::cpuRead(uint16_t addr) {
    // 1) 2 KB work RAM, mirrored every 0x800
    if (addr < 0x2000) return ram[addr & 0x07FF];

    // 2) PPU registers $2000–$2007 (mirrored through $3FFF)
    if (addr < 0x4000) return ppu->readRegister(0x2000 + (addr & 0x7));

    // 3) OAM DMA ($4014)
    if (addr == 0x4014)       return openBus();

    // 4) Controllers / APU ($4016–$4017)
    if (addr == 0x4016)       return readController();
    if (addr == 0x4017)       return readSecondController();

    // 5) APU / I/O / expansion ($4000–$5FFF)
    if (addr < 0x6000)        return openBus();

    // 6) Cartridge: PRG-RAM ($6000–$7FFF) or PRG-ROM ($8000–$FFFF)
    return mapper->cpuRead(addr);
}

void MemoryBus::cpuWrite(uint16_t addr, uint8_t val) {
    if (addr < 0x2000) {
        ram[addr & 0x07FF] = val;               return;
    }
    if (addr < 0x4000) {
        ppu->writeRegister(0x2000 + (addr & 0x7), val); return;
    }
    if (addr == 0x4014) {
        runOamDma(val);      
        cpu->stallCycles = 513;
        return;
    }
    if (addr == 0x4016) { strobeController(val); return; }
    if (addr < 0x4020)    return;  // APU / 2nd strobe
    if (addr < 0x6000)    return;  // expansion
    mapper->cpuWrite(addr, val);
}

uint8_t MemoryBus::cpuPeek(uint16_t addr) const {
    // same as cpuRead but without side-effects (eg: no OAM DMA, no strobe)
    if (addr < 0x2000) return ram[addr & 0x07FF];
    if (addr < 0x4000) return ppu->peekRegister(addr & 0x7);
    if (addr < 0x6000) return openBus();
    return mapper->cpuRead(addr);
}

uint8_t MemoryBus::ppuRead(uint16_t addr)  const {
    // mirror into 0x0000–0x3FFF
    uint16_t addr_ = addr & 0x3FFF;

    // 1) Pattern-table: $0000–$1FFF → CHR via mapper
    if (addr < 0x2000) {
        return mapper->ppuRead(addr_);
    }

    // 2) Name-tables: $2000–$2FFF mirrored through $3EFF
    if (addr < 0x3F00) {
        uint16_t m = mirrorAddress(addr_, mirrorMode);
        return nametables[m - 0x2000];
    }

    // 3) Palette: $3F00–$3F1F mirrored through $3FFF
    uint8_t p = addr_ & 0x1F;
    if ((p & 0x03) == 0) p &= 0x0F;       // universal‐background collapse
    return palette[p];
}

void MemoryBus::ppuWrite(uint16_t addr, uint8_t val) {
    uint16_t addr_ = addr & 0x3FFF;

    if (addr_ < 0x2000) {
        // CHR-RAM or CHR-ROM write
        mapper->ppuWrite(addr_, val);
    }
    else if (addr < 0x3F00) {
        // name-table write
        uint16_t m = mirrorAddress(addr_, mirrorMode);
        nametables[m - 0x2000] = val;
    }
    else {
        // palette write
        uint8_t p = addr_ & 0x1F;
        if ((p & 0x03) == 0) p &= 0x0F;
        palette[p] = val & 0x3F;
    }
}

uint8_t MemoryBus::ppuPeek(uint16_t addr)  const {
    uint16_t addr_ = addr & 0x3FFF;
    if (addr < 0x2000) {
        // Pattern tables: CHR data via mapper
        return mapper->ppuRead(addr_);
    }
    else if (addr < 0x3F00) {
        // Nametables region
        uint16_t mAddr = mirrorAddress(addr_, mirrorMode);
        return nametables[mAddr - 0x2000];
    }
    else {
        // Palette region ($3F00-$3FFF mirrors every 32 bytes)
        uint8_t p = addr_ & 0x1F;
        if ((p & 0x03) == 0) p &= 0x0F;
        return palette[p];
    }
}

//-----------------------------------------------------------------------------
// Controller support (strobe / shift register)
//-----------------------------------------------------------------------------

void MemoryBus::setButtonPressed(int bit) {
    controllerState |= (1 << bit);
}

void MemoryBus::clearButtonPressed(int bit) {
    controllerState &= ~(1 << bit);
}

uint8_t MemoryBus::readController() {
    // Return current bit, then advance if not strobed
    uint8_t ret = (controllerState >> controllerShift) & 1;
    if (!strobe) {
        controllerShift = std::min(controllerShift + 1, 7);
    }
    return ret;
}

uint8_t MemoryBus::readSecondController() {
    // Stubbed: no 2nd controller
    return 0;
}

void MemoryBus::strobeController(uint8_t val) {
    strobe = val & 1;
    if (strobe) {
        controllerShift = 0;
    }
}

//-----------------------------------------------------------------------------
// OAM DMA: copy 256 bytes from CPU page to PPU OAM
//-----------------------------------------------------------------------------

void MemoryBus::runOamDma(uint8_t page) {
    if (ppu->peekRegister(3) != 0) {
        std::printf("[DMA] OAMADDR was %02X (should be 00!)\n", ppu->peekRegister(3));
    }

    uint8_t* dst = ppu->rawOAM();
    uint16_t base = uint16_t(page) << 8;
    for (int i = 0; i < 256; ++i)
        dst[i] = cpuRead(base + i);
}

//-----------------------------------------------------------------------------
// Open-bus approximation
//-----------------------------------------------------------------------------

uint8_t MemoryBus::openBus() const {
    // Typically returns last value on the bus; stub 0
    return 0;
}

uint16_t mirrorAddress(uint16_t addr, MirrorMode mode)
{
    uint16_t nt = (addr - 0x2000) & 0x0FFF;      // 0–0xFFF after mirroring
    uint16_t table = (nt >> 10) & 3;            // 0‑3
    uint16_t offset = nt & 0x03FF;

    switch (mode) {
    case MirrorMode::HORIZONTAL: table = (table >> 1);     break; // 0,1→0  2,3→1
    case MirrorMode::VERTICAL:   table = (table & 1);      break; // 0,2→0  1,3→1
    case MirrorMode::FOUR_SCREEN:/* no change */           break; // keep 0‑3
    case MirrorMode::SINGLE_SCREEN: table = 0;             break; // mapper overrides later
    }
    return 0x2000 + (table << 10) + offset;      // always 0x2000‑0x2FFF
}
