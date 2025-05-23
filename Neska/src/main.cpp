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

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"
#include <string_view>

int main() {
    auto logger = std::make_unique<Logger>();
    auto memory = std::make_unique<MemoryBus>(logger.get());
    auto ppu = std::make_unique<PPU>(MirrorMode::HORIZONTAL, logger.get());
    auto cpu = std::make_unique<CPU>(memory.get(), ppu.get(), logger.get());

    memory->connectPPU(ppu.get());
    memory->connectCPU(cpu.get());
    ppu->setMemory(memory.get());

    MirrorMode mirror = memory->loadROM("roms/Tests/Nestest.nes");
    ppu->setMirrorMode(mirror);

    cpu->reset();
    ppu->reset();
    auto emulator = std::make_unique<Emulator>(*cpu, *ppu);

    auto renderer = std::make_unique<Renderer>
        (SCREEN_WIDTH * SCALE_FACTOR, SCREEN_HEIGHT * SCALE_FACTOR, "Neska");

    auto debugger = std::make_unique<Debugger>(*emulator, *memory);
    debugger->initGui(renderer->getSDLWindow(), renderer->getSDLRenderer());

    int skipFrames = SKIP_FRAMES;
    while (renderer->pollEvents(*memory)) {
        debugger->update();

        if (!debugger->isPaused()) {
            while (!emulator->frameComplete()) {
                emulator->step();
            }

            if (skipFrames > 0) {
                --skipFrames;
            }
            else {
                auto raw = emulator->getFrameBuffer();
                renderer->upscaleImage(raw, SCREEN_WIDTH, SCREEN_HEIGHT, SCALE_FACTOR);
                renderer->renderFrame();
            }

            emulator->resetFrameFlag();
        }

        debugger->newFrameGui();
        debugger->drawGui();
        debugger->renderGui();

        renderer->presentFrame();
        renderer->clearPixelBuffer();

        SDL_Delay(FRAME_DELAY);
    }

    debugger->shutdownGui();
    return 0;
}
