#pragma once

#include <cstdint>
#include <vector>
#include <memory>

// Base class for all mappers: handles PRG & CHR banking
class Mapper {
public:
    virtual ~Mapper() = default;

    // Initialize with PRG-ROM banks, CHR-ROM/RAM banks, and their data
    virtual void initMapper(uint8_t prgBanks,
        uint8_t chrBanks,
        const std::vector<uint8_t>& prgData,
        const std::vector<uint8_t>& chrData) = 0;

    // CPU-side access for addresses $6000–$FFFF
    virtual uint8_t cpuRead(uint16_t addr) = 0;
    virtual void    cpuWrite(uint16_t addr, uint8_t data) = 0;

    // PPU-side access for CHR $0000–$1FFF
    virtual uint8_t ppuRead(uint16_t addr) = 0;
    virtual void    ppuWrite(uint16_t addr, uint8_t data) = 0;
};

// Factory to create the appropriate mapper by ID
std::unique_ptr<Mapper> createMapper(uint8_t mapperID);

// ===========================
// Mapper0: NROM (no bank switching)
// ===========================
class Mapper0 : public Mapper {
public:
    void initMapper(uint8_t prgBanks,
        uint8_t chrBanks,
        const std::vector<uint8_t>& prgData,
        const std::vector<uint8_t>& chrData) override;
    uint8_t cpuRead(uint16_t addr) override;
    void    cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) override;
    void    ppuWrite(uint16_t addr, uint8_t data) override;

private:
    std::vector<uint8_t> prgROM;
    std::vector<uint8_t> prgRAM;   // 8 KB PRG-RAM at $6000–$7FFF
    std::vector<uint8_t> chrROM;
    uint8_t prgBanksCount = 0;
    uint8_t chrBanksCount = 0;
    bool    hasChrRam = false;
};

// ===========================
// Mapper1: MMC1
// ===========================
class Mapper1 : public Mapper {
public:
    void initMapper(uint8_t prgBanks,
        uint8_t chrBanks,
        const std::vector<uint8_t>& prgData,
        const std::vector<uint8_t>& chrData) override;
    uint8_t cpuRead(uint16_t addr) override;
    void    cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) override;
    void    ppuWrite(uint16_t addr, uint8_t data) override;

private:
    std::vector<uint8_t> prgROM;
    std::vector<uint8_t> prgRAM;
    std::vector<uint8_t> chrROM;
    uint8_t prgBanksCount = 0;
    uint8_t chrBanksCount = 0;
    bool    hasChrRam = false;

    // MMC1 registers
    uint8_t shiftReg = 0;
    int     shiftCount = 0;
    uint8_t control = 0x0C;
    uint8_t chrBank0 = 0;
    uint8_t chrBank1 = 0;
    uint8_t prgBank = 0;
    bool    prgMode = false;
    bool    chrMode = false;

    uint32_t getPRGAddress(uint16_t addr) const;
    uint32_t getCHRAddress(uint16_t addr) const;
};

// ===========================
// Mapper2: UxROM
// ===========================
class Mapper2 : public Mapper {
public:
    void initMapper(uint8_t prgBanks,
        uint8_t chrBanks,
        const std::vector<uint8_t>& prgData,
        const std::vector<uint8_t>& chrData) override;
    uint8_t cpuRead(uint16_t addr) override;
    void    cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) override;
    void    ppuWrite(uint16_t addr, uint8_t data) override;

private:
    std::vector<uint8_t> prgROM;
    std::vector<uint8_t> prgRAM;
    std::vector<uint8_t> chrROM;
    uint8_t prgBanksCount = 0;
    uint8_t chrBanksCount = 0;
    bool    hasChrRam = false;
    uint8_t bankSelect = 0;  // 16KB PRG bank at $8000
};

// ===========================
// Mapper3: CNROM
// ===========================
class Mapper3 : public Mapper {
public:
    void initMapper(uint8_t prgBanks,
        uint8_t chrBanks,
        const std::vector<uint8_t>& prgData,
        const std::vector<uint8_t>& chrData) override;
    uint8_t cpuRead(uint16_t addr) override;
    void    cpuWrite(uint16_t addr, uint8_t data) override;
    uint8_t ppuRead(uint16_t addr) override;
    void    ppuWrite(uint16_t addr, uint8_t data) override;

private:
    std::vector<uint8_t> prgROM;
    std::vector<uint8_t> prgRAM;
    std::vector<uint8_t> chrROM;
    uint8_t prgBanksCount = 0;
    uint8_t chrBanksCount = 0;
    bool    hasChrRam = false;
    uint8_t chrBankSelect = 0;  // 8KB CHR bank
};