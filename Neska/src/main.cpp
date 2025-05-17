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
    quill::Backend::start();

    quill::Logger* loggerr = quill::Frontend::create_or_get_logger(

        "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"),

        quill::PatternFormatterOptions{ "%(time) [%(thread_id)] %(short_source_location:<28) "

                                       "LOG_%(log_level:<9) %(logger:<12) %(message)",

                                       "%H:%M:%S.%Qns", quill::Timezone::GmtTime });

    LOG_INFO(loggerr, "Hello from {}!", 123);

    auto logger = std::make_unique<Logger>();
    logger->toggleLogging(true, false);

    auto memory = std::make_unique<MemoryBus>();
    auto ppu = std::make_unique<PPU>(MirrorMode::HORIZONTAL, *logger);
    auto cpu = std::make_unique<CPU>(*memory, *ppu);

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

        logger->handleLogRequests();

        SDL_Delay(FRAME_DELAY);
    }

    debugger->shutdownGui();
    return 0;
}
