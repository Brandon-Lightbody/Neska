#pragma once

#include <cstdint>
#include <iostream>
#include "memory_bus.h"
#include "ppu.h"

// 6502 status flags
static constexpr uint8_t FLAG_CARRY = 1 << 0;
static constexpr uint8_t FLAG_ZERO = 1 << 1;
static constexpr uint8_t FLAG_INTERRUPT = 1 << 2;
static constexpr uint8_t FLAG_DECIMAL = 1 << 3;
static constexpr uint8_t FLAG_BREAK = 1 << 4;
static constexpr uint8_t FLAG_UNUSED = 1 << 5;
static constexpr uint8_t FLAG_OVERFLOW = 1 << 6;
static constexpr uint8_t FLAG_NEGATIVE = 1 << 7;

// All 6502 addressing modes
enum class AddrMode {
    IMP, ACC, IMM, ZP, ZPX, ZPY,
    REL, ABS, ABX, ABY, IND, IZX, IZY
};

// Forward declare CPU
class CPU;

// Instruction descriptor
struct Instruction {
    const char* name;        // mnemonic, e.g. "LDA"
    AddrMode        mode;        // addressing mode
    uint8_t         cycles;      // base cycle count
    uint8_t(CPU::* operate)();   // core logic (returns extra cycles)
    uint16_t(CPU::* addrmode)();  // address fetch helper
};

// Forward‑declare the 256-entry table
extern Instruction instructionTable[256];

class CPU {
public:
    CPU(MemoryBus* mem, PPU* ppu);

    static void initInstructionTable();

    void requestNmi();

    void reset();
    void nmi();
    void irq();

    // Execute one instruction
    int tickCycle();

    int getTotalCycles() const;

    // Registers
    uint16_t PC;
    uint8_t  A, X, Y, SP, status;
    int      cyclesRemaining;
    uint8_t  opcode;
    uint16_t addr;     // computed address
    uint8_t  fetched;  // operand fetched

    int stallCycles;

    bool nmiRequested = false;
private:
    int totalCycles;

    MemoryBus* memory;
    PPU* ppu;

    // Bus read/write (hook these up to memory/map)
    uint8_t readByte(uint16_t addr);
    uint8_t peekByte(uint16_t a) const;
    void    writeByte(uint16_t addr, uint8_t data);

    // Addressing-mode helpers
    uint16_t addr_IMP(); uint16_t addr_ACC(); uint16_t addr_IMM();
    uint16_t addr_ZP(); uint16_t addr_ZPX(); uint16_t addr_ZPY();
    uint16_t addr_REL(); uint16_t addr_ABS(); uint16_t addr_ABX();
    uint16_t addr_ABY(); uint16_t addr_IND(); uint16_t addr_IZX();
    uint16_t addr_IZY();

    // Opcode implementations
    uint8_t ADC();  uint8_t SBC();  uint8_t AND();  uint8_t ORA();
    uint8_t EOR();  uint8_t CMP();  uint8_t CPX();  uint8_t CPY();

    uint8_t ASL(); uint8_t LSR(); uint8_t ROL(); uint8_t ROR();
    uint8_t INX(); uint8_t DEX(); uint8_t INY(); uint8_t DEY();

    uint8_t DEC(); uint8_t INC(); uint8_t BNE(); uint8_t BEQ();
    uint8_t BMI(); uint8_t BPL(); uint8_t BCS(); uint8_t BCC();

    uint8_t BIT(); uint8_t BVS(); uint8_t BVC(); uint8_t PHA();
    uint8_t PHP(); uint8_t PLA(); uint8_t PLP(); uint8_t JMP();

    uint8_t JSR(); uint8_t RTS(); uint8_t RTI(); uint8_t TAX();
    uint8_t TXA(); uint8_t TAY(); uint8_t TYA(); uint8_t TSX();
    uint8_t TXS(); uint8_t CLC(); uint8_t SEC(); uint8_t CLI();
    uint8_t SEI(); uint8_t CLV(); uint8_t CLD(); uint8_t SED();

    uint8_t LDA(); uint8_t LDX(); uint8_t LDY(); uint8_t STA();
    uint8_t STX(); uint8_t STY(); uint8_t NOP();

    uint8_t BRK();

    // Unofficial / “illegal” NMOS 6502 opcodes
    uint8_t ARR();  // AND then ROR
    uint8_t ASR();  // AND then LSR
    uint8_t ATX();  // Transfer A to X then AND
    uint8_t AXS();  // AND then store to X
    uint8_t ISC();  // INC then SBC
    uint8_t DCP();  // DEC then CMP
    uint8_t SLO();  // ASL then ORA
    uint8_t RLA();  // ROL then AND
    uint8_t SRE();  // LSR then EOR
    uint8_t RRA();  // ROR then ADC
    uint8_t LAX();  // LDA + LDX simultaneous
    uint8_t SAX();  // STA masked by X
    uint8_t LAR();  // LDA + AND SP into A
    uint8_t AXA();
    uint8_t XAS();  // store (A & X) & (high-byte(addr)+1) into memory (ABS,X)
    uint8_t SKB();
    uint8_t XAA();  // AND then transfer A→X
    uint8_t ANE();  // AND, EOR, then OR with X
    uint8_t DOP();  // double‐NOP (various lengths)
    uint8_t TOP();  // triple‐NOP (various lengths)
    uint8_t SXA();  // store X & (high byte of PC) into memory
    uint8_t SYA();  // store Y & (high byte of PC) into memory
    uint8_t ANC();  // AND then set carry = bit 7 of result


    uint8_t ILL();  // Illegal/unimplemented opcode handler

    uint8_t ROR_Helper(uint8_t value);

    // Flag helpers
    void setFlag(uint8_t mask, bool v);
    bool getFlag(uint8_t mask) const;
};