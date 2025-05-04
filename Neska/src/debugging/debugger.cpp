#include "debugger.h"
#include "debug_gui.h"

Debugger::Debugger(Emulator& emu, MemoryBus& mem)
    : emu(emu), mem(mem), cpu(*emu.getCPU()),
    ppu(*emu.getPPU()), paused(false), stepRequested(false)
{
    gui = std::make_unique<DebugGUI>(*this);

    // Start in paused mode for now.
    paused = true;
}

Debugger::~Debugger() = default;

void Debugger::initGui(SDL_Window* w, SDL_Renderer* r) {
    gui->init(w, r);
}

void Debugger::newFrameGui() {
    gui->newFrame();
}

void Debugger::drawGui() {
    gui->draw();
}

void Debugger::renderGui() {
    gui->render();
}

void Debugger::shutdownGui() {
    gui->shutdown();
}

void Debugger::update() {
    uint16_t pc = cpu.PC;
    if (breakpoints.count(pc)) paused = true;

    if (stepRequested) {
        emu.step();
        stepRequested = false;
        paused = true;
    }
    else if (!paused) {
        emu.step();
    }
}

void Debugger::requestStep() { stepRequested = true; }
void Debugger::pause() { paused = true; }
void Debugger::resume() { paused = false; }
bool Debugger::isPaused() const { return paused; }

void Debugger::toggleBreakpoint(uint16_t addr) {
    if (breakpoints.count(addr)) breakpoints.erase(addr);
    else breakpoints.insert(addr);
}

bool Debugger::hasBreakpoint(uint16_t addr) const {
    return breakpoints.count(addr) > 0;
}

const std::unordered_set<uint16_t>& Debugger::getBreakpoints() const {
    return breakpoints;
}

CPUState Debugger::getCPUState() const {
    return { cpu.PC, cpu.A, cpu.X, cpu.Y, cpu.SP, cpu.status };
}

PPUState Debugger::getPPUState() const {
    return { ppu.getScanline(), ppu.getCycle() };
}

std::vector<uint8_t> Debugger::peekMemory(uint16_t addr, size_t len) const {
    std::vector<uint8_t> out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) out.push_back(mem.cpuPeek(addr + i));
    return out;
}

std::vector<uint8_t> Debugger::peekPPUMemory(uint16_t addr, size_t len) const {
    std::vector<uint8_t> out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) out.push_back(mem.ppuPeek(addr + i));
    return out;
}
