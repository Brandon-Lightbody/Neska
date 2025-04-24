// ppu.cpp
#include "ppu.h"
#include "memory.h"
#include <cstring>
#include <iostream>

#include "logger.h"

// ----------------
// PPUFlags
// ----------------

PPUFlags::PPUFlags() {
    clear();
}

void PPUFlags::clear() {
    vblank = false;
    sprite0Hit = false;
    spriteOverflow = false;
    nmiOccurred = false;
}

uint8_t PPUFlags::toByte() const {
    return (vblank ? 0x80 : 0x00) |
        (sprite0Hit ? 0x40 : 0x00) |
        (spriteOverflow ? 0x20 : 0x00);
}

void PPUFlags::set(PPUStatusFlag flag) {
    switch (flag) {
    case PPUStatusFlag::VBlank:        vblank = true;  break;
    case PPUStatusFlag::Sprite0Hit:    sprite0Hit = true;  break;
    case PPUStatusFlag::SpriteOverflow:spriteOverflow = true;  break;
    case PPUStatusFlag::NMI:           nmiOccurred = true;  break;
    }
}

void PPUFlags::clear(PPUStatusFlag flag) {
    switch (flag) {
    case PPUStatusFlag::VBlank:        vblank = false; break;
    case PPUStatusFlag::Sprite0Hit:    sprite0Hit = false; break;
    case PPUStatusFlag::SpriteOverflow:spriteOverflow = false; break;
    case PPUStatusFlag::NMI:           nmiOccurred = false; break;
    }
}

// ----------------
// Static palette
// ----------------

const uint32_t PPU::nesPalette[64] = {
    0xFF757575,0xFF271B8F,0xFF0000AB,0xFF47009F, 0xFF8F0077,0xFFAB0013,0xFFA70000,0xFF7F0B00,
    0xFF432F00,0xFF004700,0xFF005100,0xFF003F17, 0xFF1B3F5F,0xFF000000,0xFF000000,0xFF000000,
    0xFFBCBCBC,0xFF0073EF,0xFF233BEF,0xFF8300F3, 0xFFBF00BF,0xFFCF3F7F,0xFFCF7B3F,0xFFE7AB00,
    0xFFB7CF00,0xFF7BCC00,0xFF00B700,0xFF00A1A1, 0xFFE7E7E7,0xFF000000,0xFF000000,0xFF000000,
    0xFFFFFFFF,0xFF3FBFFF,0xFF5F97FF,0xFFA78BFD, 0xFFF77BFF,0xFFFF77B7,0xFFFFB33F,0xFFFFCC3F,
    0xFF99E718,0xFF4FE453,0xFF00E757,0xFF00D7BB, 0xFF33CCCC,0xFF777777,0xFF000000,0xFF000000,
    0xFFFFFFFF,0xFFA4E7FF,0xFFB3DFFF,0xFFE3CBFF, 0xFFFFC7FF,0xFFFFC7DB,0xFFFFD7AB,0xFFFFE7A3,
    0xFFE3F79F,0xFFB3EEAB,0xFF99E7A7,0xFF99D7CF, 0xFFABF7FF,0xFFCCCCCC,0xFF000000,0xFF000000
};

// ----------------
// Constructor / reset
// ----------------

PPU::PPU(MirrorMode mode, Logger& logger)
    : mirrorMode(mode), memory(nullptr),
    cycle(0), scanline(0), v(0), t(0), fineX(0), w(false),
    readBuffer(0), nmiTriggered(false), oddFrame(false), reloadPending(false),
    evaluatedSpriteCount(0), attribShiftLo(0), attribShiftHi(0),
    patternShiftLo(0), patternShiftHi(0), nextTileID(0), nextTileAttr(0),
    nextTileLo(0), nextTileHi(0), scrollX_coarse(0), scrollY_coarse(0),
    scrollY_fine(0), sprite0HitFlag(false), sprite0HitPossible(false), spriteScanline(0)
{
    std::memset(registers, 0, sizeof(registers));
    std::memset(vram, 0, sizeof(vram));
    std::memset(oam, 0, sizeof(oam));
    std::fill(std::begin(frameBuffer), std::end(frameBuffer), 0xFF000000);
    flags.clear();
    std::memset(evaluatedSpriteIndices, 0xFF, sizeof(evaluatedSpriteIndices));
    std::memset(spriteShiftLo, 0, sizeof(spriteShiftLo));
    std::memset(spriteShiftHi, 0, sizeof(spriteShiftHi));
    std::memset(spriteXCounter, 0, sizeof(spriteXCounter));
    std::memset(spriteAttrs, 0, sizeof(spriteAttrs));

    this->logger = &logger;

    //// Init palette RAM to identity.
    //for (int i = 0; i < 0x20; i++) {
    //    vram[0x3F00 + i] = uint8_t(i & 0x3F);
    //}
}

void PPU::reset() {
    std::memset(registers, 0, sizeof(registers));
    v = t = 0;
    w = false; fineX = 0;
    readBuffer = 0;
    nmiTriggered = false;
    reloadPending = false;
    flags.clear();
    std::memset(evaluatedSpriteIndices, 0xFF, sizeof(evaluatedSpriteIndices));
    evaluatedSpriteCount = 0;

    scanline = 261;
    cycle = 0;
    oddFrame = false;
}

// ----------------
// Setup
// ----------------

void PPU::setMemory(Memory* mem) {
    memory = mem;
}

void PPU::setMirrorMode(MirrorMode mode) {
    mirrorMode = mode;
}

void PPU::setCHR(uint8_t* chrData, size_t size) {
    for (size_t i = 0; i < size && i < 0x2000; ++i) {
        vram[i] = chrData[i];
    }
}

// ----------------
// Register I/O
// ----------------

void PPU::writeRegister(uint16_t addr, uint8_t val) {
    uint8_t reg = addr & 0x7;
    registers[reg] = val;

    logger->logToConsole((const char*)addr);

    switch (reg) {
    case 0: // PPUCTRL ($2000)
        // t: ...00.. ........ = fine X scroll bank (bits 0–1 = name‑table select)
        //    ..pp.. ........ = base nametable (bits 10–11)
        t = (t & 0xF3FF) | ((val & 0x03) << 10);
        break;

    case 1: // PPUMASK ($2001)
        // just store it; masking is checked in renderPixel()
        break;

    case 3: // OAMADDR ($2003)
        // sets the OAM write index
        // registers[3] holds current OAM address
        break;

    case 4: // OAMDATA ($2004)
        // write to OAM at current index, then increment
        oam[registers[3]++] = val;
        break;

    case 5: // PPUSCROLL ($2005)
        if (!w) {
            // first write: horizontal
            fineX = val & 0x07;
            t = (t & 0xFFE0) | (val >> 3);
            w = true;
        }
        else {
            // second write: vertical
            t = (t & 0x8FFF) | ((val & 0x07) << 12);
            t = (t & 0xFC1F) | ((val & 0xF8) << 2);
            w = false;
        }
        break;

    case 6: // PPUADDR ($2006)
        if (!w) {
            // first write: high byte of t
            t = (t & 0x00FF) | ((val & 0x3F) << 8);
            w = true;
        }
        else {
            // second write: low byte, then copy to v
            t = (t & 0xFF00) | val;
            v = t;
            w = false;
        }
        break;

    case 7: // PPUDATA ($2007)
        // write to VRAM via PPU's mirror logic
        vramWrite(v & 0x3FFF, val);
        // increment v
        v += (registers[0] & 0x04) ? 32 : 1;
        break;

    default:
        // $2002/$2004/$2005/$2006 reads are handled above; nothing else to do here
        break;
    }
}

uint8_t PPU::readRegister(uint16_t addr) {
    uint8_t reg = addr & 0x7;
    uint8_t value = 0;

    switch (reg) {
    case 2: // PPUSTATUS ($2002)
        // top 3 bits = flags, bottom 5 bits = last buffered read
        value = flags.toByte() | (readBuffer & 0x1F);
        // side effects:
        flags.clear(PPUStatusFlag::VBlank);  // clear VBlank
        w = false;                           // reset write toggle
        break;

    case 4: // OAMDATA ($2004)
        // returns the byte at the current OAM address
        value = oam[registers[3]];
        break;

    case 7: { // PPUDATA ($2007)
        uint16_t addr = v & 0x3FFF;
        if (addr >= 0x3F00) {
            // Palette reads are immediate
            value = vram[mirrorAddress(addr)];
        }
        else {
            // buffered read
            value = readBuffer;
            readBuffer = vramRead(addr);
        }
        // increment v by either 1 or 32 (based on PPUCTRL bit 2)
        v += (registers[0] & 0x04) ? 32 : 1;
        break;
    }

    default:
        // $2000, $2001, $2003, $2005, $2006 are write‑only (or harmless to echo)
        value = registers[reg];
        break;
    }

    return value;
}

// ----------------
// DMA from CPU for sprites
// ----------------

void PPU::writeOAM(uint8_t data) {
    uint8_t addr = registers[3];
    oam[addr++] = data;
    registers[3] = addr;
}

// ----------------
// Main clock step
// ----------------

void PPU::stepDot() {
    bool rendering = (registers[1] & 0x18);

    // Visible scanlines (0–239) and pre‑render line (261)
    if ((scanline < 240 || scanline == 261) && rendering) {
        if (scanline == 241 && cycle == 1) {
            vblankFlag = true;
        }

        // Background fetch region: dots 1–256 and 321–336
        if ((cycle >= 1 && cycle <= 256) || (cycle >= 321 && cycle <= 336)) {

            // 1) Shift the background shifters
            updateBackgroundShifters();

            fetchBackgroundData();
        }

        // 256: increment vertical position
        if (cycle == 256) {
            incrementY();
        }
        // 257: horizontal copy from t → v, sprite evaluation
        if (cycle == 257) {
            copyX();
            evaluateSprites();
        }
        // Pre‑render line (261) dots 280–304: vertical copy from t → v
        if (scanline == 261 && cycle >= 280 && cycle <= 304) {
            copyY();
        }
    }

    // Sprite fetch pipeline (dots 321–336) happens in evaluateSprites() and your sprite setup code
    // Pixel rendering on visible scanlines
    if (scanline < 240 && cycle >= 1 && cycle <= 256) {
        renderPixel();
    }

    // VBlank start
    if (scanline == 241 && cycle == 1) {
        flags.set(PPUStatusFlag::VBlank);
        vblankLatched = false;
        if (registers[0] & 0x80) {  // NMI enabled?
            nmiTriggered = true;
            flags.set(PPUStatusFlag::NMI);
        }
    }

    // Pre‑render line reset
    if (scanline == 261 && cycle == 1) {
        flags.clear(PPUStatusFlag::Sprite0Hit);
        flags.clear(PPUStatusFlag::SpriteOverflow);
        flags.clear(PPUStatusFlag::VBlank);
        nmiTriggered = false;
        sprite0HitPossible = false;
        vblankFlag = false;
    }

    // Advance PPU dot and scanline, handle odd‑frame timing
    cycle++;
    if (cycle > 340) {
        cycle = 0;
        scanline++;
        if (scanline > 261) {
            scanline = 0;
            oddFrame = !oddFrame;
        }
    }
    // Skip cycle 0 on odd pre‑render frame when rendering is enabled
    if (scanline == 261 && cycle == 0 && rendering && oddFrame) {
        cycle = 1;
    }
}

// ----------------
// Frame finalize (no-op here)
// ----------------

void PPU::renderFrame() {
    // nothing to do: emulator polls scanline/cycle for frame boundary
}

// ----------------
// Accessors
// ----------------

const uint32_t* PPU::getFrameBuffer() const {
    return frameBuffer;
}

const uint8_t* PPU::getVRAM() const {
    return vram;
}

bool PPU::isNmiTriggered() const {
    return nmiTriggered;
}

void PPU::clearNmiFlag() {
    nmiTriggered = false;
}

// ----------------
// Helpers
// ----------------

bool PPU::renderingEnabled() const {
    return (registers[1] & 0x08) || (registers[1] & 0x10);
}

void PPU::evaluateSprites() {
    // clear out previous line’s data
    evaluatedSpriteCount = 0;
    sprite0HitPossible = false;

    const int spriteHeight = (registers[0] & 0x20) ? 16 : 8;
    const uint8_t* OAM = oam;    // primary OAM, 64 entries × 4 bytes

    // 1) scan primary OAM for sprites on this scanline
    for (int i = 0; i < 64 && evaluatedSpriteCount < 8; ++i) {
        uint8_t y = OAM[i * 4 + 0];
        // sprite’s Y in OAM is “first row - 1”
        if (scanline >= (y + 1) && scanline < (y + 1 + spriteHeight)) {
            // record the index
            evaluatedSpriteIndices[evaluatedSpriteCount++] = i;

            // sprite‐0 hit *possible* if this is sprite #0
            if (i == 0) sprite0HitPossible = true;
        }
    }

    // 2) copy those 8 entries into a small local buffer (secondary OAM)
    //    and initialize the shift/X arrays
    for (int s = 0; s < evaluatedSpriteCount; ++s) {
        int idx = evaluatedSpriteIndices[s];
        // copy 4 bytes from primary OAM
        for (int b = 0; b < 4; ++b) {
            spriteScanline[s * 4 + b] = OAM[idx * 4 + b];
        }
    }

    // 3) for each sprite, fetch its pattern data and set up shifters
    for (int s = 0; s < evaluatedSpriteCount; ++s) {
        uint8_t y = spriteScanline[s * 4 + 0];
        uint8_t tile = spriteScanline[s * 4 + 1];
        uint8_t attr = spriteScanline[s * 4 + 2];
        uint8_t xPos = spriteScanline[s * 4 + 3];
        bool    flipH = (attr & 0x40) != 0;
        bool    flipV = (attr & 0x80) != 0;
        uint8_t paletteIndex = (attr & 0x03) + 4;  // sprite palettes are $04–$07

        // which row of the sprite are we on?
        int row = scanline - (y + 1);
        if (flipV) row = spriteHeight - 1 - row;

        uint16_t addrLo, addrHi;

        if (spriteHeight == 8) {
            // 8×8 sprites use PPUCTRL bit 3 for table select
            uint16_t table = (registers[0] & 0x08) ? 0x1000 : 0x0000;
            addrLo = table + tile * 16 + row;
            addrHi = table + tile * 16 + row + 8;
        }
        else {
            // 8×16 mode: low bit of tile selects table, even/odd tile number
            uint16_t table = (tile & 1) ? 0x1000 : 0x0000;
            uint8_t  bank = tile & 0xFE;
            if (row < 8) {
                addrLo = table + bank * 16 + row;
                addrHi = table + bank * 16 + row + 8;
            }
            else {
                addrLo = table + (bank + 1) * 16 + (row - 8);
                addrHi = table + (bank + 1) * 16 + (row - 8) + 8;
            }
        }

        // fetch the two pattern bytes
        uint8_t lo = memory->ppuRead(addrLo);
        uint8_t hi = memory->ppuRead(addrHi);

        // apply H‐flip if requested
        if (flipH) {
            lo = reverse_bits(lo);
            hi = reverse_bits(hi);
        }

        // stash into our shift registers / counters / attrs
        spriteShiftLo[s] = lo;
        spriteShiftHi[s] = hi;
        spriteXCounter[s] = xPos;
        spriteAttrs[s] = attr;
    }
}

void PPU::renderPixel() {
    int x = cycle - 1;
    int y = scanline;

    // === BACKGROUND ===
    uint8_t bgPixel = 0;
    uint8_t bgPalette = 0;
    if (registers[1] & 0x08) { // BG enabled
        uint16_t mask = 0x8000 >> fineX;
        uint8_t bit0 = (patternShiftLo & mask) ? 1 : 0;
        uint8_t bit1 = (patternShiftHi & mask) ? 1 : 0;
        bgPixel = (bit1 << 1) | bit0;
        uint8_t pal0 = (attribShiftLo & mask) ? 1 : 0;
        uint8_t pal1 = (attribShiftHi & mask) ? 1 : 0;
        bgPalette = (pal1 << 1) | pal0;
        // hide left 8px if BG left‐col disabled
        if (x < 8 && !(registers[1] & 0x02)) {
            bgPixel = bgPalette = 0;
        }
    }

    // === SPRITES ===
    uint8_t spritePixel = 0;
    uint8_t spritePalette = 0;
    bool    spritePriority = false;
    bool    isSpriteZero = false;

    if (registers[1] & 0x10) { // SPRITES enabled
        // 1) count down X offsets
        for (int i = 0; i < evaluatedSpriteCount; ++i) {
            if (spriteXCounter[i] > 0) {
                --spriteXCounter[i];
            }
        }
        // 2) sample the first non‑zero sprite pixel whose counter==0
        for (int i = 0; i < evaluatedSpriteCount; ++i) {
            if (spriteXCounter[i] == 0) {
                // top bit of each 8‑bit shift reg = current pixel
                uint8_t p0 = (spriteShiftLo[i] & 0x80) >> 7;
                uint8_t p1 = (spriteShiftHi[i] & 0x80) >> 7;
                uint8_t p = (p1 << 1) | p0;
                if (p) {
                    spritePixel = p;
                    spritePalette = (spriteAttrs[i] & 0x03) + 4;
                    spritePriority = !(spriteAttrs[i] & 0x20);
                    isSpriteZero = (evaluatedSpriteIndices[i] == 0);
                    break;
                }
            }
        }
        // 3) shift registers for all “active” sprites
        for (int i = 0; i < evaluatedSpriteCount; ++i) {
            if (spriteXCounter[i] == 0) {
                spriteShiftLo[i] <<= 1;
                spriteShiftHi[i] <<= 1;
            }
        }
    }

    // === COMPOSITE ===
    uint8_t finalPixel = 0;
    uint8_t finalPalette = 0;

    if (bgPixel == 0 && spritePixel == 0) {
        // both transparent
    }
    else if (bgPixel == 0) {
        // only sprite
        finalPixel = spritePixel;
        finalPalette = spritePalette;
    }
    else if (spritePixel == 0) {
        // only background
        finalPixel = bgPixel;
        finalPalette = bgPalette;
    }
    else {
        // both opaque → priority
        if (spritePriority) {
            finalPixel = spritePixel;
            finalPalette = spritePalette;
        }
        else {
            finalPixel = bgPixel;
            finalPalette = bgPalette;
        }
        // sprite‑0 hit: both non‑zero and sprite 0 is frontmost
        if (sprite0HitPossible
            && bgPixel != 0
            && spritePixel != 0
            && isSpriteZero
            && x < 255)
        {
            flags.set(PPUStatusFlag::Sprite0Hit);
        }
    }

    // fetch color and write to frame buffer
    uint8_t colorIndex = vramRead(0x3F00 + ((finalPalette << 2) | finalPixel)) & 0x3F;
    frameBuffer[y * SCREEN_WIDTH + x] = nesPalette[colorIndex];
}

void PPU::fetchBackgroundData() {
    if ((cycle >= 1 && cycle <= 256) || (cycle >= 321 && cycle <= 336)) {
        updateBackgroundShifters();
        switch ((cycle - 1) & 7) {
        case 0: {
            // Log the v-register & the name-table address
            uint16_t nameAddr = 0x2000 | (v & 0x0FFF);
            nextTileID = vramRead(nameAddr);
            break;

        }
        case 2: {
            uint16_t addr = 0x23C0
                | (v & 0x0C00)
                | ((v >> 4) & 0x38)
                | ((v >> 2) & 0x07);
            nextTileAttr = vramRead(addr);
            break;
        }
        case 4: {
            uint8_t fineY = (v >> 12) & 7;
            uint16_t base = (registers[0] & 0x10) ? 0x1000 : 0x0000;
            nextTileLo = memory->ppuRead(base + nextTileID * 16 + fineY);
            break;
        }
        case 6: {
            uint8_t fineY = (v >> 12) & 7;
            uint16_t base = (registers[0] & 0x10) ? 0x1000 : 0x0000;
            nextTileHi = memory->ppuRead(base + nextTileID * 16 + fineY + 8);
            reloadBackgroundShifters();
            incrementX();
            break;
        }
        }
    }
}

void PPU::incrementX() {
    if ((v & 0x001F) == 31) {
        v &= ~0x001F;
        v ^= 0x0400;

    }
    else {
        v++;

    }
}

void PPU::incrementY() {
    if ((v & 0x7000) != 0x7000) {
        v += 0x1000;

    }
    else {
        v &= ~0x7000;
        int y = (v & 0x03E0) >> 5;
        if (y == 29) { y = 0; v ^= 0x0800; }
        else if (y == 31) y = 0;
        else y++;
        v = (v & ~0x03E0) | (y << 5);

    }
}

void PPU::copyX() {
    v = (v & 0x7BE0) | (t & 0x041F);
}
void PPU::copyY() {
    v = (v & 0x041F) | (t & 0x7BE0);
}

void PPU::updateBackgroundShifters() {
    if (!renderingEnabled()) return;
    patternShiftLo <<= 1; patternShiftHi <<= 1;
    attribShiftLo <<= 1; attribShiftHi <<= 1;
}

void PPU::reloadBackgroundShifters() {
    patternShiftLo = (patternShiftLo & 0xFF00) | nextTileLo;
    patternShiftHi = (patternShiftHi & 0xFF00) | nextTileHi;
    int cx = (v & 0x1F), cy = ((v >> 5) & 0x1F);
    int quad = ((cy / 2) & 1) << 1 | ((cx / 2) & 1);
    uint8_t bits = (nextTileAttr >> (quad * 2)) & 3;
    attribShiftLo = (attribShiftLo & 0xFF00) | ((bits & 1) ? 0xFF : 0);
    attribShiftHi = (attribShiftHi & 0xFF00) | ((bits & 2) ? 0xFF : 0);
}

uint16_t PPU::mirrorAddress(uint16_t addr) const {
    addr &= 0x3FFF;
    if (addr < 0x2000) return addr;
    if (addr < 0x3F00) {
        uint16_t off = (addr - 0x2000) % 0x1000;
        int     pg = off / 0x400;
        int     idx = off % 0x400;
        switch (mirrorMode) {
        case MirrorMode::HORIZONTAL:
            if (pg == 1) pg = 0;
            else if (pg >= 2) pg = 1;
            break;
        case MirrorMode::VERTICAL:
            if (pg >= 2) pg -= 2;
            break;
        case MirrorMode::FOUR_SCREEN:
            break;
        case MirrorMode::SINGLE_SCREEN:
            pg = 0;
            break;
        }
        return 0x2000 + pg * 0x400 + idx;
    }
    return 0x3F00 + (addr % 0x20);
}

uint8_t PPU::vramRead(uint16_t addr) const {
    addr &= 0x3FFF;
    // --- CHR ($0000–$1FFF) comes from the cartridge/mapper ---
    if (addr < 0x2000) {
        return memory->ppuRead(addr);
    }
    // --- nametables ($2000–$2FFF) and palette ($3F00–$3F1F) live in vram[] ---
    uint16_t m = mirrorAddress(addr);
    if (m < 0x3F00)
        return vram[m];
    else
        // palette mirrors every 32 bytes
        return vram[0x3F00 + (m & 0x1F)];
}

void PPU::vramWrite(uint16_t addr, uint8_t val) {
    addr &= 0x3FFF;
    // --- CHR writes must go back into CHR‐RAM (if present) or be ignored ---
    if (addr < 0x2000) {
        memory->ppuWrite(addr, val);
        return;
    }
    // --- name‐table / palette land in the PPU’s own RAM ---
    uint16_t m = mirrorAddress(addr);
    if (m < 0x3F00)
        vram[m] = val;
    else
        vram[0x3F00 + (m & 0x1F)] = val;
}

uint8_t PPU::reverse_bits(uint8_t b) {
    uint8_t r = 0; for (int i = 0; i < 8; i++) if (b & (1 << i)) r |= 1 << (7 - i);
    return r;
}

bool PPU::isVBlank() const {
    // Only latch once
    return vblankFlag && !vblankLatched;
}

bool PPU::nmiOutputEnabled() const {
    return (registers[0] & 0x80) != 0;
}

void PPU::clearVBlank() {
    flags.clear(PPUStatusFlag::VBlank);
    vblankLatched = true;
}
