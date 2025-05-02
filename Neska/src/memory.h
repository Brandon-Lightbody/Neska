#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include "core.h"
#include "mapper.h"

class CPU;
class PPU;

class Memory {
public:
    Memory();

    void setPPU(PPU* p);
    void setCPU(CPU* p);

    MirrorMode loadROM(const std::string& path, std::vector<uint8_t>& chrRomOut);

    uint8_t read(uint16_t addr);
    void    write(uint16_t addr, uint8_t val);

    uint8_t peek(uint16_t addr) const;

    uint8_t readFromMapper(uint16_t addr);
    void writeToMapper(uint16_t addr, uint8_t val);

    uint8_t ppuBusRead(uint16_t addr)  const;
    void ppuBusWrite(uint16_t addr, uint8_t val) const;

    void setButtonPressed(int bit);
    void clearButtonPressed(int bit);
private:
    std::vector<uint8_t> ram;
    bool    strobe;
    uint8_t controllerState;
    uint8_t controllerShift;

    PPU* ppu;
    CPU* cpu;

    std::unique_ptr<Mapper> mapper;

    uint8_t readController();
    uint8_t readSecondController();
    void    strobeController(uint8_t val);
    void    runOamDma(uint8_t page);
    uint8_t openBus() const;
};