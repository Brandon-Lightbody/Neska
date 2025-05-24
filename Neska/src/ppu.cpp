#include "ppu.h"
#include "memory_bus.h"
#include <cstring>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

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

PPU::PPU(MirrorMode mode)
    : mirrorMode(mode), memory(nullptr),
    cycle(0), scanline(0), v(0), t(0), fineX(0), w(false),
    readBuffer(0), nmiTriggered(false), oddFrame(false),
    evaluatedSpriteCount(0), attribShiftLo(0), attribShiftHi(0),
    patternShiftLo(0), patternShiftHi(0), nextTileID(0), nextTileAttr(0),
    nextTileLo(0), nextTileHi(0), scrollX_coarse(0), scrollY_coarse(0),
    scrollY_fine(0), sprite0HitFlag(false), sprite0HitPossible(false),
    vblankLatched(true), vblankFlag(false), nmiPending(false), nmiOutput(false)
{
    std::memset(registers, 0, sizeof(registers));
    std::memset(oam, 0, sizeof(oam));
    std::fill(std::begin(frameBuffer), std::end(frameBuffer), 0xFF000000);
    flags.clear();
    std::memset(evaluatedSpriteIndices, 0xFF, sizeof(evaluatedSpriteIndices));
    std::memset(spriteShiftLo, 0, sizeof(spriteShiftLo));
    std::memset(spriteShiftHi, 0, sizeof(spriteShiftHi));
    std::memset(spriteXCounter, 0, sizeof(spriteXCounter));
    std::memset(spriteAttrs, 0, sizeof(spriteAttrs));
}

void PPU::reset() {
    std::memset(registers, 0, sizeof(registers));
    v = t = 0;
    w = false;
    fineX = 0;
    readBuffer = 0;
    nmiTriggered = false;
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

void PPU::setMemory(MemoryBus* mem) {
    memory = mem;
}

void PPU::setMirrorMode(MirrorMode mode) {
    mirrorMode = mode;
}

// ----------------
// Register I/O
// ----------------

void PPU::writeRegister(uint16_t addr, uint8_t val) {
    uint8_t reg = addr & 0x7;
    switch (reg) {
    case 0: {  // PPUCTRL ($2000)
        bool oldNmiOut = nmiOutput;
        registers[0] = val;
        nmiOutput = (val & 0x80) != 0;
        // Update temporary VRAM address: bits 10–11 = nametable select
        t = (t & 0xF3FF) | ((val & 0x03) << 10);
        // If NMI was just enabled during VBlank, schedule it
        if (!oldNmiOut && nmiOutput && vblankFlag) {
            nmiPending = true;
        }
        break;
    }
    case 1: { // PPUMASK ($2001)
        registers[1] = val;
        break;
    }
    case 3: { // OAMADDR ($2003)
        registers[3] = val;
        break;
    }
    case 4: { // OAMDATA ($2004)
        registers[4] = val;
        oam[registers[3]++] = val;
        break;
    }
    case 5: {  // PPUSCROLL ($2005)
        if (!w) {
            fineX = val & 0x07;
            t = (t & 0xFFE0) | (val >> 3);
            w = true;
        }
        else {
            t = (t & 0x8FFF) | ((val & 0x07) << 12);
            t = (t & 0xFC1F) | ((val & 0xF8) << 2);
            w = false;
        }
        break;
    }
    case 6: {  // PPUADDR ($2006)
        registers[6] = val;
        if (!w) {
            t = (t & 0x00FF) | ((val & 0x3F) << 8);
            w = true;
        }
        else {
            t = (t & 0xFF00) | val;
            v = t;
            w = false;
        }
        break;
    }
    case 7: {  // PPUDATA ($2007)
        registers[7] = val;
        uint16_t vaddr = v & 0x3FFF;
        int inc = (registers[0] & 0x04) ? 32 : 1;
        memory->ppuWrite(vaddr, val);
        v += inc;
        break;
    }
    default:
        // Writes to other regs ($2002/$2004/$2005/$2006 reads are irrelevant here)
        registers[reg] = val;
        break;
    }
}

uint8_t PPU::peekRegister(uint16_t addr) const {
    return registers[addr & 0x7];
}

uint8_t PPU::readRegister(uint16_t addr) {
    uint8_t reg = addr & 0x7;
    uint8_t value = 0;

    static int statusReadsThisFrame = 0;

    switch (reg) {
    case 2: { // PPUSTATUS ($2002)
        value = flags.toByte() | (readBuffer & 0x1F);
        // clear the vblank flag, but no longer reset 'w' here:
        clearVBlank();
        w = false;
        break;
    }
    case 4: { // OAMDATA ($2004)
        // returns the byte at the current OAM address
        value = oam[registers[3]];
        break;
    }
    case 7: { // PPUDATA ($2007)
        uint8_t data;
        if (v >= 0x3F00) {
            // palette reads are immediate
            data = memory->ppuRead(v);

        }
        else {
            // buffered read: return old buffer, then refill
            data = readBuffer;
            readBuffer = memory->ppuRead(v);
        }
        value = data;

        // advance v (1 or 32)
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
    oam[registers[3]++] = data;
}

uint8_t* PPU::rawOAM() {
    return oam;
}

// ----------------
// Main clock step
// ----------------

void PPU::stepDot()
{
    // ──────────────────────────────────────────────────────────────────────
    // 0) Pre‑render line reset  (scanline 261, dot 1)
    // ──────────────────────────────────────────────────────────────────────
    if (scanline == 261 && cycle == 1) {
        flags.clear(PPUStatusFlag::Sprite0Hit);
        flags.clear(PPUStatusFlag::SpriteOverflow);
        flags.clear(PPUStatusFlag::VBlank);
        sprite0HitPossible = false;
        vblankFlag = false;
        nmiTriggered = false;

        w = false;                                // PPUSCROLL/PPUADDR latch
        patternShiftLo = patternShiftHi = 0;
        attribShiftLo = attribShiftHi = 0;

        logInfo("[PPU] Frame start.");
    }

    // ──────────────────────────────────────────────────────────────────────
    // 1) Cached “rendering on?” bits
    // ──────────────────────────────────────────────────────────────────────
    const bool bgEnable = (registers[1] & 0x08) != 0;
    const bool spEnable = (registers[1] & 0x10) != 0;
    const bool rendering = bgEnable || spEnable;

    // ──────────────────────────────────────────────────────────────────────
    // 2) Background fetch / shifter pipeline
    // ──────────────────────────────────────────────────────────────────────
    if (renderingEnabled() &&
        ((scanline < 240 && cycle >= 1 && cycle <= 256) ||   // visible
            (cycle >= 321 && cycle <= 336) ||   // tile pre‑fetch
            (scanline == 261 && cycle >= 321 && cycle <= 336)))    // pre‑render pre‑fetch
    {
        updateBackgroundShifters();
        fetchBackgroundData();
    }

    // ──────────────────────────────────────────────────────────────────────
    // 3) Scroll bookkeeping (runs if *either* BG or sprites are on)
    // ──────────────────────────────────────────────────────────────────────
    if (rendering) {
        if (cycle == 256) {
            incrementY();
        }
        if (cycle == 257) {
            copyX();                         // dot 257
            if (scanline < 240 || scanline == 261) {
                evaluateSprites();
            }
        }
        if (scanline == 261 && cycle >= 280 && cycle <= 304) {
            copyY();
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // 4) Render a pixel   (visible scanlines, dots 1‑256)
    // ──────────────────────────────────────────────────────────────────────
    if (scanline < 240 && cycle >= 1 && cycle <= 256) {
        renderPixel();
    }

    // ──────────────────────────────────────────────────────────────────────
    // 5) Enter VBlank             (scanline 241, dot 1)
    // ──────────────────────────────────────────────────────────────────────
    if (scanline == 241 && cycle == 1) {
        flags.set(PPUStatusFlag::VBlank);
        vblankFlag = true;
        vblankLatched = false;

        if (nmiOutputEnabled()) {                 // PPUCTRL bit 7
            flags.set(PPUStatusFlag::NMI);
            nmiTriggered = true;
        }

        logInfo("[PPU] Frame complete.");
    }

    // ──────────────────────────────────────────────────────────────────────
    // 6) Odd‑frame cycle‑339 shortcut (only when rendering)
    // ──────────────────────────────────────────────────────────────────────
    if (scanline == 261 && cycle == 0 && rendering && oddFrame) {
        cycle = 0;            // skip dot 340 → start new frame immediately
        scanline = 0;
        oddFrame = !oddFrame;
        return;               // early exit: frame finished
    }

    // ──────────────────────────────────────────────────────────────────────
    // 7) Advance dot & scanline counters
    // ──────────────────────────────────────────────────────────────────────
    ++cycle;
    if (cycle > 340) {
        cycle = 0;
        ++scanline;
        if (scanline > 261) {
            scanline = 0;
            oddFrame = !oddFrame;
        }
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

const uint8_t* PPU::getFrameBuffer() const {
    return frameBuffer.data();
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

    std::fill_n(spriteShiftLo, 8, 0);
    std::fill_n(spriteShiftHi, 8, 0);
    std::fill_n(spriteAttrs, 8, 0);
    std::fill_n(spriteXCounter, 8, 0xFF);

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
        for (int s = evaluatedSpriteCount; s < 8; ++s) {
            spriteShiftLo[s] = 0;
            spriteShiftHi[s] = 0;
            spriteXCounter[s] = 0xFF;   // never reaches 0
            spriteAttrs[s] = 0;
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

    uint8_t colorIndex = memory->ppuRead(0x3F00 + ((finalPalette << 2) | finalPixel)) & 0x3F;
    frameBuffer[y * SCREEN_WIDTH + x] = colorIndex;
}

void PPU::fetchBackgroundData() {
    if ((cycle >= 1 && cycle <= 256) || (cycle >= 321 && cycle <= 336)) {
        switch ((cycle - 1) & 7) {
        case 0: {
            // Log the v-register & the name-table address
            uint16_t nameAddr = 0x2000 | (v & 0x0FFF);
            nextTileID = memory->ppuRead(nameAddr);
            break;
        }
        case 2: {
            if (scanline < 240) {
                uint16_t attrAddr = 0x23C0
                    | (v & 0x0C00)
                    | ((v >> 4) & 0x38)
                    | ((v >> 2) & 0x07);
                nextTileAttr = memory->ppuRead(attrAddr);
            }
            else {
                // still perform the fetch so PPU logic stays correct
                uint16_t attrAddr = 0x23C0
                    | (v & 0x0C00)
                    | ((v >> 4) & 0x38)
                    | ((v >> 2) & 0x07);
                nextTileAttr = memory->ppuRead(attrAddr);
            }
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
    // clear just bits 0–4 and bit 10 in v, then OR in t’s bits there
    v = (v & ~0x041F)    // keep all other bits
        | (t & 0x041F);   // copy coarse X + nametable
}
void PPU::copyY() {
    // clear fine Y, coarse Y, and nametable‐Y (bits 5–9+11), then OR from t
    v = (v & ~0x7BE0)
        | (t & 0x7BE0);
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

uint8_t PPU::reverse_bits(uint8_t b) {
    uint8_t r = 0; for (int i = 0; i < 8; i++) if (b & (1 << i)) r |= 1 << (7 - i);
    return r;
}

bool PPU::isVBlank() const {
    // Only latch once
    return vblankFlag && !vblankLatched;
}

bool PPU::nmiOutputEnabled() const {
    bool ret = (registers[0] & 0x80) != 0;
    logInfo("[PPU] nmiOutputEnabled() returned.");
    return ret;
}

void PPU::clearVBlank() {
    flags.clear(PPUStatusFlag::VBlank);
    vblankLatched = true;
}
