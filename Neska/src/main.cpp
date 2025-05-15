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
    auto logger = std::make_unique<Logger>();
    logger->toggleLogging(true, false);

    auto memory = std::make_unique<MemoryBus>();
    auto ppu = std::make_unique<PPU>(MirrorMode::HORIZONTAL, *logger);
    auto cpu = std::make_unique<CPU>(*memory, *ppu);

    memory->connectPPU(ppu.get());
    memory->connectCPU(cpu.get());
    ppu->setMemory(memory.get());

    MirrorMode mirror = memory->loadROM("roms/PAC-MAN.nes");
    ppu->setMirrorMode(mirror);

    cpu->reset();
    ppu->reset();
    auto emulator = std::make_unique<Emulator>(*cpu, *ppu);

    Renderer renderer(
        SCREEN_WIDTH * SCALE_FACTOR,
        SCREEN_HEIGHT * SCALE_FACTOR,
        "Neska"
    );

    // 6) Debugger + GUI
    Debugger debugger(*emulator, *memory);
    debugger.initGui(renderer.getSDLWindow(), renderer.getSDLRenderer());

    // 7) Main loop (skip the first two frames, then render normally)
    int skipFrames = SKIP_FRAMES;
    while (renderer.pollEvents(*memory)) {
        debugger.update();

        if (!debugger.isPaused()) {
            // a) Run until the PPU signals end-of-frame
            while (!emulator->frameComplete()) {
                emulator->step();
            }

            // b) If we’ve skipped fewer than two, just consume the frame
            if (skipFrames > 0) {
                --skipFrames;
            }
            else {
                auto raw = emulator->getFrameBuffer();
                renderer.upscaleImage(raw, SCREEN_WIDTH, SCREEN_HEIGHT, SCALE_FACTOR);
                renderer.renderFrame();
            }

            // d) Prepare for the next frame
            emulator->resetFrameFlag();
        }

        // 8) GUI + Present + Throttle
        debugger.newFrameGui();
        debugger.drawGui();
        debugger.renderGui();

        renderer.presentFrame();
        renderer.clearPixelBuffer();

        logger->handleLogRequests();

        SDL_Delay(FRAME_DELAY);
    }

    // 9) Cleanup
    debugger.shutdownGui();
    return 0;
}
