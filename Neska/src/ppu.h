#pragma once

#include <cstdint>
#include <vector>
#include <cstring>
#include <iostream>
#include "debugging/logger.h"
#include "core.h"

struct PPUFlags {
    bool vblank = false;
    bool sprite0Hit = false;
    bool spriteOverflow = false;
    bool nmiOccurred = false;

    PPUFlags();
    void clear();
    uint8_t toByte() const;
    void set(PPUStatusFlag flag);
    void clear(PPUStatusFlag flag);
};

// Forward declaration of Memory.
class MemoryBus;

class PPU {
public:
    // Constructor.
    explicit PPU(MirrorMode mode, Logger& logger);

    void reset();

    // Set pointer to the shared Memory.
    void setMemory(MemoryBus* mem);

    // Update mirror mode.
    void setMirrorMode(MirrorMode mode);

    // CPU register read/write (0x2000-0x2007).
    uint8_t readRegister(uint16_t addr);
    void writeRegister(uint16_t addr, uint8_t value);

    uint8_t peekRegister(uint16_t addr) const;

    void stepDot(); // single PPU clock (dot) step

    // Finalize the frame once VBlank is done.
    void renderFrame();

    // Access the final 256x240 RGBA buffer.
    const uint8_t* getFrameBuffer() const;

    // For sync
    int getScanline() const { return scanline; }
    int getCycle()    const { return cycle; }

    // NMI: triggered when entering VBlank.
    bool isNmiTriggered() const;
    void clearNmiFlag();

    // CPU OAM DMA writes.
    void writeOAM(uint8_t data);
    uint8_t* rawOAM();

    // For the NES palette.
    static const uint32_t nesPalette[64];

    void updateBackgroundShifters();
    void reloadBackgroundShifters();
    bool renderingEnabled() const;

    // Helpers.
    bool isVBlank() const;
    bool nmiOutputEnabled() const;
    void clearVBlank();
private:
    // Background pipeline functions.
    void fetchBackgroundData();
    void incrementX();
    void incrementY();
    void copyX();
    void copyY();

    // Render one pixel (dot) for background.
    void renderPixel();

    void evaluateSprites();
private:
    Logger* logger;

    bool vblankFlag;
    bool vblankLatched;

    // PPU registers (0-7, mirrored).
    uint8_t registers[8];

    // OAM memory (sprite RAM), 256 bytes.
    uint8_t oam[256];

    uint8_t frameBuffer[SCREEN_WIDTH * SCREEN_HEIGHT];

    // Loopy registers and internal variables.
    uint16_t v;    // current VRAM address.
    uint16_t t;    // temporary VRAM address.
    uint8_t fineX; // fine horizontal scroll.
    bool w;        // write toggle.

    // Read buffer for PPUDATA.
    uint8_t readBuffer;

    // PPU cycle and scanline counters.
    int cycle;    // 0 to 340 (or 339 on odd frames pre-render)
    int scanline; // 0 to 261

    // VBlank flag and NMI trigger.
    bool nmiTriggered;

    bool nmiOutput;
    bool nmiPending;

    // Odd-frame flag (for even/odd frame timing).
    bool oddFrame;

    // Mirroring mode.
    MirrorMode mirrorMode;

    PPUFlags flags;

    // Pointer to Memory (for mapper and CHR data).
    MemoryBus* memory;

    // Background shift registers and latches.
    uint16_t patternShiftLo;
    uint16_t patternShiftHi;
    uint16_t attribShiftLo;
    uint16_t attribShiftHi;
    uint8_t nextTileID;
    uint8_t nextTileAttr;
    uint8_t nextTileLo;
    uint8_t nextTileHi;

    uint8_t spriteShiftLo[8], spriteShiftHi[8];
    uint8_t spriteXCounter[8], spriteAttrs[8];
    bool    sprite0HitPossible;

    uint8_t reverse_bits(uint8_t b);

    // Scroll registers (set via PPUSCROLL writes).
    uint8_t scrollX_coarse;
    uint8_t scrollY_coarse;
    uint8_t scrollY_fine;

    // *** NEW: Sprite evaluation data ***
    int evaluatedSpriteCount;          // Number of sprites on the current scanline (max 8)
    int evaluatedSpriteIndices[8];     // OAM indices of the evaluated sprites.            // Flag for sprite 0 hit on the current scanline.
    uint8_t spriteScanline[32]{};
    bool sprite0HitFlag;
};