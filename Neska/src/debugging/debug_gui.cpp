#include "debug_gui.h"
#include "debugger.h"

#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

DebugGUI::DebugGUI(Debugger& dbg)
    : dbg(dbg), memViewAddr(0x0000),
    sdlWindow(nullptr), sdlRenderer(nullptr)
{
}

DebugGUI::~DebugGUI() {
    shutdown();
}

void DebugGUI::init(SDL_Window* window, SDL_Renderer* renderer) {
    sdlWindow = window;
    sdlRenderer = renderer;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForSDLRenderer(sdlWindow, sdlRenderer);
    ImGui_ImplSDLRenderer3_Init(sdlRenderer);
}

void DebugGUI::newFrame() {
    ImGui_ImplSDL3_NewFrame();
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui::NewFrame();
}

void DebugGUI::draw() {
    // CPU window
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::Begin("CPU");
    auto s = dbg.getCPUState();
    ImGui::Text("PC: 0x%04X", s.PC);
    ImGui::Text("A: %02X   X: %02X   Y: %02X", s.A, s.X, s.Y);
    ImGui::Text("SP: %02X   STATUS: %02X", s.SP, s.status);
    if (ImGui::Button(dbg.isPaused() ? "Run" : "Pause")) {
        dbg.isPaused() ? dbg.resume() : dbg.pause();
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        dbg.requestStep();
    }
    ImGui::Separator();
    ImGui::Text("Breakpoints:");
    ImGui::BeginChild("bps", ImVec2(0, 100), true);
    for (auto bp : dbg.getBreakpoints()) {
        ImGui::Text("0x%04X", bp);
    }
    ImGui::EndChild();
    ImGui::InputScalar("Toggle BP at", ImGuiDataType_U16,
        &memViewAddr, nullptr, nullptr, "%04X");
    ImGui::SameLine();
    if (ImGui::Button("Toggle BP")) {
        dbg.toggleBreakpoint(memViewAddr);
    }
    ImGui::End();

    // Memory window
    ImGui::SetNextWindowPos(ImVec2(530, 10));
    ImGui::Begin("Memory");
    ImGui::InputScalar("Addr", ImGuiDataType_U16,
        &memViewAddr, nullptr, nullptr, "%04X");
    auto block = dbg.readMemory(memViewAddr, 256);
    for (int i = 0; i < 16; ++i) {
        for (int j = 0; j < 16; ++j) {
            ImGui::Text("%02X ", block[i * 16 + j]);
            ImGui::SameLine();
        }
        ImGui::NewLine();
    }
    ImGui::End();

    // PPU window
    ImGui::SetNextWindowPos(ImVec2(10, 270));
    ImGui::Begin("PPU");
    auto p = dbg.getPPUState();
    ImGui::Text("Scanline: %d   Cycle: %d", p.scanline, p.cycle);
    ImGui::End();
}

void DebugGUI::render() {
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdlRenderer);
}

void DebugGUI::shutdown() {
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}
