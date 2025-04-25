// main.cpp
#include <memory>
#include <iostream>
#include <fstream>
#include <iomanip>

#include "memory.h"
#include "ppu.h"
#include "cpu.h"
#include "emulator.h"
#include "renderer/cpu/cpu_renderer.h"
#include "debugging/logger.h"

int main() {
    auto logger = std::make_unique<Logger>();
    logger->toggleLogging(true, false);

    // 1) Construct core subsystems
    auto memory = std::make_unique<Memory>();
    auto ppu = std::make_unique<PPU>(MirrorMode::HORIZONTAL, *logger);
    auto cpu = std::make_unique<CPU>(*memory, *ppu);

    // 2) Wire them together
    memory->setPPU(ppu.get());
    memory->setCPU(cpu.get());
    ppu->setMemory(memory.get());

    // 3) Load the ROM (header→PRG→CHR) and get its mirroring mode
    std::vector<uint8_t> chrData;
    MirrorMode mirror = memory->loadROM("roms/donkey_kong.nes", chrData);
    ppu->setMirrorMode(mirror);
    ppu->setCHR(chrData.data(), chrData.size());

    // 4) Reset CPU & PPU to start executing the game's reset/vector code
    cpu->reset();   // loads PC from $FFFC/$FFFD
    ppu->reset();   // clears all internal state

    Emulator emu(*cpu, *ppu);

    // 7) Create SDL window/renderer
    Renderer renderer(SCREEN_WIDTH * 4,
        SCREEN_HEIGHT * 4,
        "NES Emulator");

    // 8) Main loop: step until a frame is done, then draw it
    while (renderer.pollEvents(*memory)) {
        while (!emu.frameComplete()) {
            emu.step();
        }

        // Grab the 256×240 ARGB buffer and upscale 4× for the window
        const uint32_t* rawFrame = emu.getFrameBuffer();
        auto scaled = renderer.upscaleImage(rawFrame,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            4);

        renderer.renderFrame(scaled.data());
        emu.resetFrameFlag();
        
        logger->handleLogRequests();

        SDL_Delay(16);  // ~60 Hz
    }

    return 0;
}
