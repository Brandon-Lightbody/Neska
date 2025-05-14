#include <memory>
#include <iostream>
#include <iomanip>

#include "debugging/Debugger.h"
#include "debugging/logger.h"

#include "memory_bus.h"
#include "ppu.h"
#include "cpu.h"
#include "emulator.h"
#include "renderer/cpu/cpu_renderer.h"

int main() {
    // 1) Logger
    auto logger = std::make_unique<Logger>();
    logger->toggleLogging(true, false);

    // 2) Core subsystems
    auto memory = std::make_unique<MemoryBus>();
    auto ppu = std::make_unique<PPU>(MirrorMode::HORIZONTAL, *logger);
    auto cpu = std::make_unique<CPU>(*memory, *ppu);

    memory->connectPPU(ppu.get());
    memory->connectCPU(cpu.get());
    ppu->setMemory(memory.get());

    // 3) Load ROM
    MirrorMode mirror = memory->loadROM("roms/Donkey Kong.nes");
    ppu->setMirrorMode(mirror);

    // 4) Reset CPU/PPU and wrap them in the emulator
    cpu->reset();
    ppu->reset();
    Emulator emu(*cpu, *ppu);

    // 5) Renderer
    constexpr int SCALE = 4;
    Renderer renderer(
        SCREEN_WIDTH * SCALE,
        SCREEN_HEIGHT * SCALE,
        "Neska"
    );

    // 6) Debugger + GUI
    Debugger debugger(emu, *memory);
    debugger.initGui(renderer.getSDLWindow(), renderer.getSDLRenderer());

    // 7) Main loop (skip the first two frames, then render normally)
    const int FRAME_DELAY = 1000 / 60;
    int skipFrames = 60;
    while (renderer.pollEvents(*memory)) {
        debugger.update();

        if (!debugger.isPaused()) {
            // a) Run until the PPU signals end-of-frame
            while (!emu.frameComplete()) {
                emu.step();
            }

            // b) If we’ve skipped fewer than two, just consume the frame
            if (skipFrames > 0) {
                --skipFrames;
            }
            else {
                // c) Now that v and the name‐table are initialized, draw!
                auto raw = emu.getFrameBuffer();
                auto scaled = renderer.upscaleImage(
                    raw, SCREEN_WIDTH, SCREEN_HEIGHT, SCALE
                );
                renderer.renderFrame(scaled.data());
            }

            // d) Prepare for the next frame
            emu.resetFrameFlag();
        }

        // 8) GUI + Present + Throttle
        debugger.newFrameGui();
        debugger.drawGui();
        debugger.renderGui();

        renderer.present();
        logger->handleLogRequests();
        SDL_Delay(FRAME_DELAY);
    }

    // 9) Cleanup
    debugger.shutdownGui();
    return 0;
}
