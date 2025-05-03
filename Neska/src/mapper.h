#pragma once

#include <cstdint>
#include <vector>
#include <memory>

class Mapper {
public:
    virtual ~Mapper() = default;

    virtual void init(uint8_t prgBanks, uint8_t chrBanks,
        const std::vector<uint8_t>& prgData,
        const std::vector<uint8_t>& chrData);

    virtual uint8_t cpuRead(uint16_t addr);
    virtual void cpuWrite(uint16_t addr, uint8_t value);

    virtual uint8_t ppuRead(uint16_t addr);
    virtual void ppuWrite(uint16_t addr, uint8_t value);
};

std::unique_ptr<Mapper> createMapper(uint8_t mapperID);

class NROM : public Mapper {
public:
    void init(uint8_t prgBanks, uint8_t chrBanks,
        const std::vector<uint8_t>& prgData,
        const std::vector<uint8_t>& chrData) override;

    uint8_t cpuRead(uint16_t addr) override;
    void cpuWrite(uint16_t addr, uint8_t value) override;

    uint8_t ppuRead(uint16_t addr) override;
    void ppuWrite(uint16_t addr, uint8_t value) override;
private:
    std::vector<uint8_t> prgROM;
    std::vector<uint8_t> prgRAM;
    std::vector<uint8_t> chrRom;
    std::vector<uint8_t> chrRam;
    bool hasChrRam = false;
    uint8_t prgBanksCount = 0;
    uint8_t chrBanksCount = 0;
};

class MMC1 : public Mapper {
public:
    void init(uint8_t prgBanks, uint8_t chrBanks,
        const std::vector<uint8_t>& prgData,
        const std::vector<uint8_t>& chrData) override;

    uint8_t cpuRead(uint16_t addr) override;
    void    cpuWrite(uint16_t addr, uint8_t value) override;

    uint8_t ppuRead(uint16_t addr) override;
    void    ppuWrite(uint16_t addr, uint8_t value) override;

private:
    // cart data
    std::vector<uint8_t> prgROM, chrROM, chrRAM;
    bool hasChrRAM = false;
    uint8_t prgBanks = 0, chrBanks = 0;

    // MMC1 registers
    uint8_t shiftReg = 0x10;   // 5‑bit register (bit‑4 always 1 => “empty”)
    uint8_t control = 0x0C;   // startup: PRG 16 KB switching, vertical mir.
    uint8_t chrBank0 = 0;
    uint8_t chrBank1 = 0;
    uint8_t prgBank = 0;

    // helpers
    void commitRegister(uint16_t addr);
};

class UNROM : public Mapper {
public:
    void init(uint8_t prgBanks, uint8_t chrBanks,
        const std::vector<uint8_t>& prgData,
        const std::vector<uint8_t>& chrData) override;

    uint8_t cpuRead(uint16_t addr) override;
    void    cpuWrite(uint16_t addr, uint8_t value) override;

    uint8_t ppuRead(uint16_t addr) override;
    void    ppuWrite(uint16_t addr, uint8_t value) override;

private:
    std::vector<uint8_t> prgROM, chrRAM;
    uint8_t prgBanks = 0, bankSelect = 0;
};

class CNROM : public Mapper {
public:
    void init(uint8_t prgBanks, uint8_t chrBanks,
        const std::vector<uint8_t>& prgData,
        const std::vector<uint8_t>& chrData) override;

    uint8_t cpuRead(uint16_t addr) override;
    void    cpuWrite(uint16_t addr, uint8_t value) override;

    uint8_t ppuRead(uint16_t addr) override;
    void    ppuWrite(uint16_t addr, uint8_t value) override;

private:
    std::vector<uint8_t> prgROM, chrROM;
    uint8_t prgBanks = 0, chrBanks = 0, chrBankSelect = 0;
};