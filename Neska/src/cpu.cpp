#include "cpu.h"

#include "debugging/logger.h"

// 256-entry instruction table
Instruction instructionTable[256];

// Macro to set an entry
#define SET_INS(op, mnem, mode, cyc) \
    instructionTable[op] = { #mnem, AddrMode::mode, cyc, &CPU::mnem, &CPU::addr_##mode };

// Table initializer runs before main()
struct TableInitializer { TableInitializer() { CPU::initInstructionTable(); } } tableInitializer;

// Constructor
CPU::CPU(MemoryBus* mem, PPU* ppu)
    : memory(mem), ppu(ppu), PC(0), A(0), X(0), Y(0), SP(0xFD),
    status(FLAG_UNUSED), cyclesRemaining(0), opcode(0), addr(0), fetched(0),
    stallCycles(0), nmiRequested(false), totalCycles(0)
{
}

void CPU::requestNmi() {
    nmiRequested = true;
}

// Reset CPU and set PC from reset vector
void CPU::reset() {
    SP = 0xFD;
    status = FLAG_UNUSED | FLAG_INTERRUPT;  // set I=1 on reset
    cyclesRemaining = 0;
    stallCycles = 0;
    uint16_t lo = readByte(0xFFFC);
    uint16_t hi = readByte(0xFFFD);
    PC = (hi << 8) | lo;
}

void CPU::nmi() {
    // push PC high, PC low, FLAGS with B=0 & U=1
    writeByte(0x0100 + SP--, (PC >> 8) & 0xFF);
    writeByte(0x0100 + SP--, PC & 0xFF);
    writeByte(0x0100 + SP--, (status & ~FLAG_BREAK) | FLAG_UNUSED);

    // read vector, set PC
    uint16_t lo = readByte(0xFFFA);
    uint16_t hi = readByte(0xFFFB);
    PC = (hi << 8) | lo;

    // NMIs cost 7 cycles total
    cyclesRemaining += 7;

    setFlag(FLAG_INTERRUPT, true);
}

void CPU::irq() {
    if (getFlag(FLAG_INTERRUPT)) return; // masked
    writeByte(0x0100 + SP--, (PC >> 8) & 0xFF);
    writeByte(0x0100 + SP--, PC & 0xFF);
    writeByte(0x0100 + SP--, (status & ~FLAG_BREAK) | FLAG_UNUSED);

    uint16_t lo = readByte(0xFFFE);
    uint16_t hi = readByte(0xFFFF);
    PC = (hi << 8) | lo;

    // IRQ also costs 7 cycles
    cyclesRemaining += 7;

    setFlag(FLAG_INTERRUPT, true);
}

int CPU::tickCycle() {
    // Handle DMA stall cycles first
    if (stallCycles > 0) {
        --stallCycles;
        ++totalCycles;
        return 1;
    }

    // If we're starting a new instruction
    if (cyclesRemaining == 0) {
        // Handle any pending interrupts before executing instructions
        if (nmiRequested) {
            nmiRequested = false;
            nmi();
        }
        else if (!getFlag(FLAG_INTERRUPT)) {
            irq();
        }

        // Fetch next opcode
        opcode = readByte(PC++);
        const Instruction& ins = instructionTable[opcode];

        // Base cycle count for the instruction
        cyclesRemaining = ins.cycles;

        // Addressing mode resolution (may add a page-cross penalty)
        bool pageCross = (this->*ins.addrmode)();

        // Add additional cycle if page crossing on certain addressing modes
        if (pageCross &&
            (ins.mode == AddrMode::ABX ||
                ins.mode == AddrMode::ABY ||
                ins.mode == AddrMode::REL)) {
            ++cyclesRemaining;
        }

        // Execute instruction logic (may add additional cycles)
        cyclesRemaining += (this->*ins.operate)();
    }

    // Consume a CPU cycle
    --cyclesRemaining;
    ++totalCycles;

    return 1; // Always return at least 1 cycle consumed per tick
}

int CPU::getTotalCycles() const {
    return totalCycles;
}

// Initialize table with defaults and specific entries
void CPU::initInstructionTable() {
    // Default all to illegal
    for (int i = 0; i < 256; i++) {
        instructionTable[i] = { "ILL", AddrMode::IMP, 2, &CPU::ILL, &CPU::addr_IMP };
    }
    // ADC, SBC, AND, ORA, EOR, CMP, CPX, CPY
    SET_INS(0x69, ADC, IMM, 2); SET_INS(0x65, ADC, ZP, 3); SET_INS(0x75, ADC, ZPX, 4);
    SET_INS(0x6D, ADC, ABS, 4); SET_INS(0x7D, ADC, ABX, 4); SET_INS(0x79, ADC, ABY, 4);
    SET_INS(0x61, ADC, IZX, 6); SET_INS(0x71, ADC, IZY, 5);
    SET_INS(0xE9, SBC, IMM, 2); SET_INS(0xE5, SBC, ZP, 3); SET_INS(0xF5, SBC, ZPX, 4);
    SET_INS(0xED, SBC, ABS, 4); SET_INS(0xFD, SBC, ABX, 4); SET_INS(0xF9, SBC, ABY, 4);
    SET_INS(0xE1, SBC, IZX, 6); SET_INS(0xF1, SBC, IZY, 5);
    SET_INS(0x29, AND, IMM, 2); SET_INS(0x25, AND, ZP, 3); SET_INS(0x35, AND, ZPX, 4);
    SET_INS(0x2D, AND, ABS, 4); SET_INS(0x3D, AND, ABX, 4); SET_INS(0x39, AND, ABY, 4);
    SET_INS(0x21, AND, IZX, 6); SET_INS(0x31, AND, IZY, 5);
    SET_INS(0x09, ORA, IMM, 2); SET_INS(0x05, ORA, ZP, 3); SET_INS(0x15, ORA, ZPX, 4);
    SET_INS(0x0D, ORA, ABS, 4); SET_INS(0x1D, ORA, ABX, 4); SET_INS(0x19, ORA, ABY, 4);
    SET_INS(0x01, ORA, IZX, 6); SET_INS(0x11, ORA, IZY, 5);
    SET_INS(0x49, EOR, IMM, 2); SET_INS(0x45, EOR, ZP, 3); SET_INS(0x55, EOR, ZPX, 4);
    SET_INS(0x4D, EOR, ABS, 4); SET_INS(0x5D, EOR, ABX, 4); SET_INS(0x59, EOR, ABY, 4);
    SET_INS(0x41, EOR, IZX, 6); SET_INS(0x51, EOR, IZY, 5);
    SET_INS(0xC9, CMP, IMM, 2); SET_INS(0xC5, CMP, ZP, 3); SET_INS(0xD5, CMP, ZPX, 4);
    SET_INS(0xCD, CMP, ABS, 4); SET_INS(0xDD, CMP, ABX, 4); SET_INS(0xD9, CMP, ABY, 4);
    SET_INS(0xC1, CMP, IZX, 6); SET_INS(0xD1, CMP, IZY, 5);
    SET_INS(0xE0, CPX, IMM, 2); SET_INS(0xE4, CPX, ZP, 3); SET_INS(0xEC, CPX, ABS, 4);
    SET_INS(0xC0, CPY, IMM, 2); SET_INS(0xC4, CPY, ZP, 3); SET_INS(0xCC, CPY, ABS, 4);

    // Shifts
    SET_INS(0x0A, ASL, ACC, 2); SET_INS(0x06, ASL, ZP, 5); SET_INS(0x16, ASL, ZPX, 6); SET_INS(0x0E, ASL, ABS, 6); SET_INS(0x1E, ASL, ABX, 7);
    SET_INS(0x4A, LSR, ACC, 2); SET_INS(0x46, LSR, ZP, 5); SET_INS(0x56, LSR, ZPX, 6); SET_INS(0x4E, LSR, ABS, 6); SET_INS(0x5E, LSR, ABX, 7);
    SET_INS(0x2A, ROL, ACC, 2); SET_INS(0x26, ROL, ZP, 5); SET_INS(0x36, ROL, ZPX, 6); SET_INS(0x2E, ROL, ABS, 6); SET_INS(0x3E, ROL, ABX, 7);
    SET_INS(0x6A, ROR, ACC, 2); SET_INS(0x66, ROR, ZP, 5); SET_INS(0x76, ROR, ZPX, 6); SET_INS(0x6E, ROR, ABS, 6); SET_INS(0x7E, ROR, ABX, 7);

    // Register inc/dec
    SET_INS(0xE8, INX, IMP, 2); SET_INS(0xCA, DEX, IMP, 2); SET_INS(0xC8, INY, IMP, 2); SET_INS(0x88, DEY, IMP, 2);

    SET_INS(0xC6, DEC, ZP, 5); SET_INS(0xD6, DEC, ZPX, 6); SET_INS(0xCE, DEC, ABS, 6); SET_INS(0xDE, DEC, ABX, 7);
    SET_INS(0xE6, INC, ZP, 5); SET_INS(0xF6, INC, ZPX, 6); SET_INS(0xEE, INC, ABS, 6); SET_INS(0xFE, INC, ABX, 7);

    // Branches
    SET_INS(0xD0, BNE, REL, 2); SET_INS(0xF0, BEQ, REL, 2);
    SET_INS(0x30, BMI, REL, 2); SET_INS(0x10, BPL, REL, 2);
    SET_INS(0xB0, BCS, REL, 2); SET_INS(0x90, BCC, REL, 2);

    SET_INS(0x24, BIT, ZP, 3); SET_INS(0x2C, BIT, ABS, 4); SET_INS(0x70, BVS, REL, 2);
    SET_INS(0x50, BVC, REL, 2); SET_INS(0x48, PHA, IMP, 3); SET_INS(0x08, PHP, IMP, 3);
    SET_INS(0x68, PLA, IMP, 4); SET_INS(0x28, PLP, IMP, 4); SET_INS(0x4C, JMP, ABS, 3);
    SET_INS(0x6C, JMP, IND, 5);

    SET_INS(0x20, JSR, ABS, 6); SET_INS(0x60, RTS, IMP, 6); SET_INS(0x40, RTI, IMP, 6);
    SET_INS(0xAA, TAX, IMP, 2); SET_INS(0x8A, TXA, IMP, 2); SET_INS(0xA8, TAY, IMP, 2);
    SET_INS(0x98, TYA, IMP, 2); SET_INS(0xBA, TSX, IMP, 2); SET_INS(0x9A, TXS, IMP, 2);
    SET_INS(0x18, CLC, IMP, 2); SET_INS(0x38, SEC, IMP, 2); SET_INS(0x58, CLI, IMP, 2);
    SET_INS(0x78, SEI, IMP, 2); SET_INS(0xB8, CLV, IMP, 2); SET_INS(0xD8, CLD, IMP, 2);
    SET_INS(0xF8, SED, IMP, 2);

    SET_INS(0xA9, LDA, IMM, 2); SET_INS(0xA5, LDA, ZP, 3); SET_INS(0xB5, LDA, ZPX, 4);
    SET_INS(0xAD, LDA, ABS, 4); SET_INS(0xBD, LDA, ABX, 4); SET_INS(0xB9, LDA, ABY, 4);
    SET_INS(0xA1, LDA, IZX, 6); SET_INS(0xB1, LDA, IZY, 5);

    SET_INS(0xA2, LDX, IMM, 2); SET_INS(0xA6, LDX, ZP, 3); SET_INS(0xB6, LDX, ZPY, 4);
    SET_INS(0xAE, LDX, ABS, 4); SET_INS(0xBE, LDX, ABY, 4);

    SET_INS(0xA0, LDY, IMM, 2); SET_INS(0xA4, LDY, ZP, 3); SET_INS(0xB4, LDY, ZPX, 4);
    SET_INS(0xAC, LDY, ABS, 4); SET_INS(0xBC, LDY, ABX, 4);

    // stores
    SET_INS(0x85, STA, ZP, 3); SET_INS(0x95, STA, ZPX, 4); SET_INS(0x8D, STA, ABS, 4);
    SET_INS(0x9D, STA, ABX, 5); SET_INS(0x99, STA, ABY, 5); SET_INS(0x81, STA, IZX, 6);
    SET_INS(0x91, STA, IZY, 6);

    SET_INS(0x86, STX, ZP, 3); SET_INS(0x96, STX, ZPY, 4); SET_INS(0x8E, STX, ABS, 4);
    SET_INS(0x84, STY, ZP, 3); SET_INS(0x94, STY, ZPX, 4); SET_INS(0x8C, STY, ABS, 4);

    // NOP
    SET_INS(0xEA, NOP, IMP, 2);

    ////////
    // —— Single‐byte “alias” opcodes
    SET_INS(0x6B, ARR, IMM, 2);
    SET_INS(0x4B, ASR, IMM, 2);
    SET_INS(0xAB, ATX, IMM, 2);
    SET_INS(0xCB, AXS, IMM, 2);
    SET_INS(0xEB, SBC, IMM, 2);

    // —— AXA “7th‐bit” AND/store
    SET_INS(0x9F, AXA, ABY, 5);
    SET_INS(0x93, AXA, IZY, 6);

    // —— DCP “DEC then CMP”
    SET_INS(0xC7, DCP, ZP, 5);
    SET_INS(0xD7, DCP, ZPX, 6);
    SET_INS(0xCF, DCP, ABS, 6);
    SET_INS(0xDF, DCP, ABX, 7);
    SET_INS(0xDB, DCP, ABY, 7);
    SET_INS(0xC3, DCP, IZX, 8);
    SET_INS(0xD3, DCP, IZY, 8);

    // —— ISC “INC then SBC”
    SET_INS(0xE7, ISC, ZP, 5);
    SET_INS(0xF7, ISC, ZPX, 6);
    SET_INS(0xEF, ISC, ABS, 6);
    SET_INS(0xFF, ISC, ABX, 7);
    SET_INS(0xFB, ISC, ABY, 7);
    SET_INS(0xE3, ISC, IZX, 8);
    SET_INS(0xF3, ISC, IZY, 8);

    // —— Double‐NOP (“DOP” / “SKB”)
    SET_INS(0x04, DOP, ZP, 3);
    SET_INS(0x14, DOP, ZPX, 4);
    SET_INS(0x34, DOP, ZPX, 4);
    SET_INS(0x44, DOP, ZP, 3);
    SET_INS(0x54, DOP, ZPX, 4);
    SET_INS(0x64, DOP, ZP, 3);
    SET_INS(0x74, DOP, ZPX, 4);
    SET_INS(0x80, DOP, IMM, 2);
    SET_INS(0x82, DOP, IMM, 2);
    SET_INS(0x89, DOP, IMM, 2);
    SET_INS(0xC2, DOP, IMM, 2);
    SET_INS(0xD4, DOP, ZPX, 4);
    SET_INS(0xE2, DOP, IMM, 2);
    SET_INS(0xF4, DOP, ZPX, 4);

    // —— RLA “ROL then AND”
    SET_INS(0x27, RLA, ZP, 5);
    SET_INS(0x37, RLA, ZPX, 6);
    SET_INS(0x2F, RLA, ABS, 6);
    SET_INS(0x3F, RLA, ABX, 7);
    SET_INS(0x3B, RLA, ABY, 7);
    SET_INS(0x23, RLA, IZX, 8);
    SET_INS(0x33, RLA, IZY, 8);

    // —— RRA “ROR then ADC”
    SET_INS(0x67, RRA, ZP, 5);
    SET_INS(0x77, RRA, ZPX, 6);
    SET_INS(0x6F, RRA, ABS, 6);
    SET_INS(0x7F, RRA, ABX, 7);
    SET_INS(0x7B, RRA, ABY, 7);
    SET_INS(0x63, RRA, IZX, 8);
    SET_INS(0x73, RRA, IZY, 8);

    // —— SLO “ASL then ORA”
    SET_INS(0x07, SLO, ZP, 5);
    SET_INS(0x17, SLO, ZPX, 6);
    SET_INS(0x0F, SLO, ABS, 6);
    SET_INS(0x1F, SLO, ABX, 7);
    SET_INS(0x1B, SLO, ABY, 7);
    SET_INS(0x03, SLO, IZX, 8);
    SET_INS(0x13, SLO, IZY, 8);

    // —— SRE “LSR then EOR”
    SET_INS(0x47, SRE, ZP, 5);
    SET_INS(0x57, SRE, ZPX, 6);
    SET_INS(0x4F, SRE, ABS, 6);
    SET_INS(0x5F, SRE, ABX, 7);
    SET_INS(0x5B, SRE, ABY, 7);
    SET_INS(0x43, SRE, IZX, 8);
    SET_INS(0x53, SRE, IZY, 8);

    // —— DCP alias: “SAX” / “LAX”
    SET_INS(0xA7, LAX, ZP, 3);
    SET_INS(0xB7, LAX, ZPY, 4);
    SET_INS(0xAF, LAX, ABS, 4);
    SET_INS(0xBF, LAX, ABY, 4);
    SET_INS(0xA3, LAX, IZX, 6);
    SET_INS(0xB3, LAX, IZY, 5);

    // —— LAR “AND SP with mem, then LDX/LDA/SP”
    SET_INS(0xBB, LAR, ABY, 4);

    // —— AXA / SHA weird store (7th‐bit)
    SET_INS(0x9E, SXA, ABY, 5);
    SET_INS(0x9C, SYA, ABX, 5);

    // —— Triple‐NOP (“TOP” / “SKW”)
    SET_INS(0x0C, TOP, ABS, 4);
    SET_INS(0x1C, TOP, ABX, 4);
    SET_INS(0x3C, TOP, ABX, 4);
    SET_INS(0x5C, TOP, ABX, 4);
    SET_INS(0x7C, TOP, ABX, 4);
    SET_INS(0xDC, TOP, ABX, 4);
    SET_INS(0xFC, TOP, ABX, 4);

    // —— XAA / ANE family
    SET_INS(0x8B, XAA, IMM, 2); 
    SET_INS(0x9B, XAS, ABY, 5);

    SET_INS(0xFA, SKB, ABY, 4);
     
    // —— ANC (ALR) “AND then set carry from bit 7”
    SET_INS(0x0B, ANC, IMM, 2);
    SET_INS(0x2B, ANC, IMM, 2);

    // —— SAX “store A & X”
    SET_INS(0x87, SAX, ZP, 3);
    SET_INS(0x97, SAX, ZPY, 4);
    SET_INS(0x8F, SAX, ABS, 4);
    SET_INS(0x83, SAX, IZX, 6);

    // —— single‐byte NOPs
    SET_INS(0x1A, NOP, IMP, 2);
    SET_INS(0x3A, NOP, IMP, 2);
    SET_INS(0x5A, NOP, IMP, 2);
    SET_INS(0x7A, NOP, IMP, 2);
    SET_INS(0xDA, NOP, IMP, 2);
}

// Addressing modes (return true if page crossed for ABX/ABY)
uint16_t CPU::addr_IMP() { 
    fetched = A;
    return 0;
}

uint16_t CPU::addr_ACC() {
    fetched = A;
    return 0;
}

uint16_t CPU::addr_IMM() {
    addr = PC++;
    fetched = readByte(addr);
    return 0;
}


uint16_t CPU::addr_ZP() {
    addr = readByte(PC++) & 0xFF;
    auto op = instructionTable[opcode].operate;
    if (op == &CPU::STA || op == &CPU::STX || op == &CPU::STY ||
        op == &CPU::SAX || op == &CPU::XAS) {
        fetched = peekByte(addr);
    }
    else {
        fetched = readByte(addr);
    }
    return 0;
}

uint16_t CPU::addr_ZPX() {
    addr = (readByte(PC++) + X) & 0xFF;
    auto op = instructionTable[opcode].operate;
    if (op == &CPU::STA || op == &CPU::STX || op == &CPU::STY ||
        op == &CPU::SAX || op == &CPU::XAS) {
        fetched = peekByte(addr);
    }
    else {
        fetched = readByte(addr);
    }
    return 0;
}

uint16_t CPU::addr_ZPY() {
    addr = (readByte(PC++) + Y) & 0xFF;
    auto op = instructionTable[opcode].operate;
    if (op == &CPU::STA || op == &CPU::STX || op == &CPU::STY ||
        op == &CPU::SAX || op == &CPU::XAS) {
        fetched = peekByte(addr);
    }
    else {
        fetched = readByte(addr);
    }
    return 0;
}

uint16_t CPU::addr_REL() {
    int8_t o = (int8_t)readByte(PC++);
    uint16_t prev = PC; addr = PC + o;
    fetched = 0;
    return ((prev & 0xFF00) != (addr & 0xFF00));
}

uint16_t CPU::addr_ABS() {
    uint16_t lo = readByte(PC++);
    uint16_t hi = readByte(PC++);
    addr = (hi << 8) | lo;
    auto op = instructionTable[opcode].operate;
    if (op == &CPU::STA || op == &CPU::STX || op == &CPU::STY ||
        op == &CPU::SAX || op == &CPU::XAS) {
        fetched = peekByte(addr);
    }
    else {
        fetched = readByte(addr);
    }
    return 0;
}

uint16_t CPU::addr_ABX() {
    uint16_t lo = readByte(PC++);
    uint16_t hi = readByte(PC++);
    uint16_t base = (hi << 8) | lo;
    addr = base + X;
    bool pageCross = ((base & 0xFF00) != (addr & 0xFF00));
    auto op = instructionTable[opcode].operate;
    if (op == &CPU::STA || op == &CPU::STX || op == &CPU::STY ||
        op == &CPU::SAX || op == &CPU::XAS) {
        fetched = peekByte(addr);
    }
    else {
        fetched = readByte(addr);
    }
    return pageCross;
}

uint16_t CPU::addr_ABY() {
    uint16_t lo = readByte(PC++);
    uint16_t hi = readByte(PC++);
    uint16_t base = (hi << 8) | lo;
    addr = base + Y;
    bool pageCross = ((base & 0xFF00) != (addr & 0xFF00));
    auto op = instructionTable[opcode].operate;
    if (op == &CPU::STA || op == &CPU::STX || op == &CPU::STY ||
        op == &CPU::SAX || op == &CPU::XAS) {
        fetched = peekByte(addr);
    }
    else {
        fetched = readByte(addr);
    }
    return pageCross;
}


uint16_t CPU::addr_IND() {
    uint16_t ptrLo = readByte(PC++);
    uint16_t ptrHi = readByte(PC++);
    uint16_t ptr = (ptrHi << 8) | ptrLo;
    uint16_t lo = readByte(ptr);
    uint16_t hi = readByte((ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
    addr = (hi << 8) | lo;
    // JMP uses indirect and never writes; always a safe read
    fetched = readByte(addr);
    return 0;
}

uint16_t CPU::addr_IZX() {
    uint16_t zp = (readByte(PC++) + X) & 0xFF;
    uint16_t lo = readByte(zp);
    uint16_t hi = readByte((zp + 1) & 0xFF);
    addr = (hi << 8) | lo;
    auto op = instructionTable[opcode].operate;
    if (op == &CPU::STA || op == &CPU::STX || op == &CPU::STY ||
        op == &CPU::SAX || op == &CPU::XAS) {
        fetched = peekByte(addr);
    }
    else {
        fetched = readByte(addr);
    }
    return 0;
}

uint16_t CPU::addr_IZY() {
    uint16_t zp = readByte(PC++) & 0xFF;
    uint16_t lo = readByte(zp);
    uint16_t hi = readByte((zp + 1) & 0xFF);
    uint16_t base = (hi << 8) | lo;
    addr = base + Y;
    bool pageCross = ((base & 0xFF00) != (addr & 0xFF00));
    auto op = instructionTable[opcode].operate;
    if (op == &CPU::STA || op == &CPU::STX || op == &CPU::STY ||
        op == &CPU::SAX || op == &CPU::XAS) {
        fetched = peekByte(addr);
    }
    else {
        fetched = readByte(addr);
    }
    return pageCross;
}

// Bus operations with PPU mapping
uint8_t CPU::readByte(uint16_t a) {
    // ALL addresses—including $2000–$3FFF—go through Memory.
    return memory->cpuRead(a);
}

uint8_t CPU::peekByte(uint16_t a) const {
    return memory->cpuPeek(a);
}

void CPU::writeByte(uint16_t a, uint8_t d) {
    memory->cpuWrite(a, d);
}

void CPU::setFlag(uint8_t mask, bool v) { if (v) status |= mask; else status &= ~mask; }
bool CPU::getFlag(uint8_t mask) const { return (status & mask) != 0; }

/////////////////////////
// Opcode implementations
/////////////////////////
uint8_t CPU::ADC() {
    uint16_t sum = A + fetched + (getFlag(FLAG_CARRY) ? 1 : 0);
    bool carry = sum > 0xFF;
    bool overflow = (~(A ^ fetched) & (A ^ sum) & 0x80) != 0;
    if (getFlag(FLAG_DECIMAL)) {
        // BCD adjust
        uint8_t lo = (A & 0x0F) + (fetched & 0x0F) + (getFlag(FLAG_CARRY) ? 1 : 0);
        if (lo > 9) lo += 6;
        bool c2 = lo > 0x0F;
        uint8_t hi = (A >> 4) + (fetched >> 4) + (c2 ? 1 : 0);
        if (hi > 9) { hi += 6; carry = true; }
        sum = (hi << 4) | (lo & 0x0F);
        cyclesRemaining += 1;
    }
    A = sum & 0xFF;
    setFlag(FLAG_CARRY, carry);
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    setFlag(FLAG_OVERFLOW, overflow);
    return 0;
}

uint8_t CPU::SBC() {
    uint16_t value = (uint16_t)fetched ^ 0x00FF;
    uint16_t temp = (uint16_t)A + value + (getFlag(FLAG_CARRY) ? 1 : 0);
    setFlag(FLAG_CARRY, temp & 0xFF00);
    setFlag(FLAG_ZERO, (temp & 0xFF) == 0);
    setFlag(FLAG_NEGATIVE, temp & 0x80);
    setFlag(FLAG_OVERFLOW, ((temp ^ (uint16_t)A) & (temp ^ value) & 0x80) != 0);
    A = temp & 0xFF;
    return 0;
}

uint8_t CPU::AND() {
    A &= fetched;
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    return 0;
}

uint8_t CPU::ORA() {
    A |= fetched;
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    return 0;
}

uint8_t CPU::EOR() {
    A ^= fetched;
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    return 0;
}

uint8_t CPU::CMP() {
    uint16_t temp = (uint16_t)A - (uint16_t)fetched;
    setFlag(FLAG_CARRY, A >= fetched);
    setFlag(FLAG_ZERO, (temp & 0xFF) == 0);
    setFlag(FLAG_NEGATIVE, temp & 0x80);
    return 0;
}

uint8_t CPU::CPX() {
    uint16_t temp = (uint16_t)X - (uint16_t)fetched;
    setFlag(FLAG_CARRY, X >= fetched);
    setFlag(FLAG_ZERO, (temp & 0xFF) == 0);
    setFlag(FLAG_NEGATIVE, temp & 0x80);
    return 0;
}

uint8_t CPU::CPY() {
    uint16_t temp = (uint16_t)Y - (uint16_t)fetched;
    setFlag(FLAG_CARRY, Y >= fetched);
    setFlag(FLAG_ZERO, (temp & 0xFF) == 0);
    setFlag(FLAG_NEGATIVE, temp & 0x80);
    return 0;
}

uint8_t CPU::ASL() {
    uint8_t value = (instructionTable[opcode].mode == AddrMode::ACC) ? A : fetched;
    uint8_t result = value << 1;
    setFlag(FLAG_CARRY, (value & 0x80) != 0);
    setFlag(FLAG_ZERO, result == 0);
    setFlag(FLAG_NEGATIVE, (result & 0x80) != 0);
    if (instructionTable[opcode].mode == AddrMode::ACC) A = result;
    else writeByte(addr, result);
    return 0;
}

uint8_t CPU::LSR() {
    uint8_t value = (instructionTable[opcode].mode == AddrMode::ACC) ? A : fetched;
    uint8_t result = value >> 1;
    setFlag(FLAG_CARRY, (value & 0x01) != 0);
    setFlag(FLAG_ZERO, result == 0);
    setFlag(FLAG_NEGATIVE, false);
    if (instructionTable[opcode].mode == AddrMode::ACC) A = result;
    else writeByte(addr, result);
    return 0;
}

uint8_t CPU::ROL() {
    uint8_t value = (instructionTable[opcode].mode == AddrMode::ACC) ? A : fetched;
    uint8_t carryIn = getFlag(FLAG_CARRY) ? 1 : 0;
    uint8_t result = (value << 1) | carryIn;
    setFlag(FLAG_CARRY, (value & 0x80) != 0);
    setFlag(FLAG_ZERO, result == 0);
    setFlag(FLAG_NEGATIVE, (result & 0x80) != 0);
    if (instructionTable[opcode].mode == AddrMode::ACC) A = result;
    else writeByte(addr, result);
    return 0;
}

uint8_t CPU::ROR() {
    uint8_t value = (instructionTable[opcode].mode == AddrMode::ACC) ? A : fetched;
    uint8_t carryIn = getFlag(FLAG_CARRY) ? 0x80 : 0;
    uint8_t result = (value >> 1) | carryIn;
    setFlag(FLAG_CARRY, (value & 0x01) != 0);
    setFlag(FLAG_ZERO, result == 0);
    setFlag(FLAG_NEGATIVE, (result & 0x80) != 0);
    if (instructionTable[opcode].mode == AddrMode::ACC) A = result;
    else writeByte(addr, result);
    return 0;
}

uint8_t CPU::INX() {
    X++;
    setFlag(FLAG_ZERO, X == 0);
    setFlag(FLAG_NEGATIVE, (X & 0x80) != 0);
    return 0;
}

uint8_t CPU::DEX() {
    X--;
    setFlag(FLAG_ZERO, X == 0);
    setFlag(FLAG_NEGATIVE, (X & 0x80) != 0);
    return 0;
}

uint8_t CPU::INY() {
    Y++;
    setFlag(FLAG_ZERO, Y == 0);
    setFlag(FLAG_NEGATIVE, (Y & 0x80) != 0);
    return 0;
}

uint8_t CPU::DEY() {
    Y--;
    setFlag(FLAG_ZERO, Y == 0);
    setFlag(FLAG_NEGATIVE, (Y & 0x80) != 0);
    return 0;
}

uint8_t CPU::DEC() { uint8_t val = fetched - 1; writeByte(addr, val); setFlag(FLAG_ZERO, val == 0); setFlag(FLAG_NEGATIVE, (val & 0x80) != 0); return 0; }
uint8_t CPU::INC() { uint8_t val = fetched + 1; writeByte(addr, val); setFlag(FLAG_ZERO, val == 0); setFlag(FLAG_NEGATIVE, (val & 0x80) != 0); return 0; }

uint8_t CPU::BNE() { if (!getFlag(FLAG_ZERO)) { uint16_t old = PC; PC = addr; int extra = 1; if ((old & 0xFF00) != (PC & 0xFF00)) extra++; return extra; } return 0; }
uint8_t CPU::BEQ() { if (getFlag(FLAG_ZERO)) { uint16_t old = PC; PC = addr; int extra = 1; if ((old & 0xFF00) != (PC & 0xFF00)) extra++; cyclesRemaining += extra - 1; return extra; } return 0; }
uint8_t CPU::BMI() { if (getFlag(FLAG_NEGATIVE)) { uint16_t old = PC; PC = addr; int extra = 1; if ((old & 0xFF00) != (PC & 0xFF00)) extra++; cyclesRemaining += extra - 1; return extra; } return 0; }
uint8_t CPU::BPL() { if (!getFlag(FLAG_NEGATIVE)) { uint16_t old = PC; PC = addr; int extra = 1; if ((old & 0xFF00) != (PC & 0xFF00)) extra++; cyclesRemaining += extra - 1; return extra; } return 0; }
uint8_t CPU::BCS() { if (getFlag(FLAG_CARRY)) { uint16_t old = PC; PC = addr; int extra = 1; if ((old & 0xFF00) != (PC & 0xFF00)) extra++; cyclesRemaining += extra - 1; return extra; } return 0; }
uint8_t CPU::BCC() { if (!getFlag(FLAG_CARRY)) { uint16_t old = PC; PC = addr; int extra = 1; if ((old & 0xFF00) != (PC & 0xFF00)) extra++; cyclesRemaining += extra - 1; return extra; } return 0; }

uint8_t CPU::BIT() {
    setFlag(FLAG_ZERO, (A & fetched) == 0);
    setFlag(FLAG_NEGATIVE, (fetched & 0x80) != 0);
    setFlag(FLAG_OVERFLOW, (fetched & 0x40) != 0);
    return 0;
}

uint8_t CPU::BVS() {
    if (getFlag(FLAG_OVERFLOW)) {
        uint16_t old = PC;
        PC = addr;
        uint8_t extra = 1 + (((old & 0xFF00) != (PC & 0xFF00)) ? 1 : 0);
        cyclesRemaining += extra - 1;
        return extra;
    }
    return 0;
}

uint8_t CPU::BVC() {
    
    if (!getFlag(FLAG_OVERFLOW)) {
        uint16_t old = PC;
        PC = addr;
        int extra = 1 + (((old & 0xFF00) != (PC & 0xFF00)) ? 1 : 0);
        cyclesRemaining += extra - 1;
        return extra;
    }
    return 0;
}

uint8_t CPU::PHA() {
    writeByte(0x0100 + SP--, A);
    return 0;
}

uint8_t CPU::PHP() {
    writeByte(0x0100 + SP--, status | FLAG_BREAK | FLAG_UNUSED);
    return 0;
}

uint8_t CPU::PLA() {
    uint8_t val = readByte(0x0100 + ++SP);
    A = val;
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, (A & 0x80) != 0);
    return 0;
}

uint8_t CPU::PLP() {
    uint8_t val = readByte(0x0100 + ++SP);
    status = val;
    return 0;
}

uint8_t CPU::JMP() {
    PC = addr;
    return 0;
}

uint8_t CPU::JSR() {
    uint16_t ret = PC - 1;
    writeByte(0x0100 + SP--, (ret >> 8) & 0xFF);
    writeByte(0x0100 + SP--, ret & 0xFF);
    PC = addr;
    return 0;
}

uint8_t CPU::RTS() {
    uint8_t lo = readByte(0x0100 + ++SP);
    uint8_t hi = readByte(0x0100 + ++SP);
    PC = ((hi << 8) | lo) + 1;
    return 0;
}

uint8_t CPU::RTI() {
    status = readByte(0x0100 + ++SP);
    uint8_t lo = readByte(0x0100 + ++SP);
    uint8_t hi = readByte(0x0100 + ++SP);
    PC = (hi << 8) | lo;
    return 0;
}

uint8_t CPU::TAX() { A = X; setFlag(FLAG_ZERO, A == 0); setFlag(FLAG_NEGATIVE, A & 0x80); return 0; }
uint8_t CPU::TXA() { X = A; setFlag(FLAG_ZERO, X == 0); setFlag(FLAG_NEGATIVE, X & 0x80); return 0; }
uint8_t CPU::TAY() { A = Y; setFlag(FLAG_ZERO, A == 0); setFlag(FLAG_NEGATIVE, A & 0x80); return 0; }
uint8_t CPU::TYA() { Y = A; setFlag(FLAG_ZERO, Y == 0); setFlag(FLAG_NEGATIVE, Y & 0x80); return 0; }
uint8_t CPU::TSX() { X = SP; setFlag(FLAG_ZERO, X == 0); setFlag(FLAG_NEGATIVE, X & 0x80); return 0; }
uint8_t CPU::TXS() { SP = X; return 0; }

uint8_t CPU::CLC() { setFlag(FLAG_CARRY, false); return 0; }
uint8_t CPU::SEC() { setFlag(FLAG_CARRY, true); return 0; }
uint8_t CPU::CLI() { setFlag(FLAG_INTERRUPT, false); return 0; }
uint8_t CPU::SEI() { setFlag(FLAG_INTERRUPT, true); return 0; }
uint8_t CPU::CLV() { setFlag(FLAG_OVERFLOW, false); return 0; }
uint8_t CPU::CLD() { setFlag(FLAG_DECIMAL, false); return 0; }
uint8_t CPU::SED() { setFlag(FLAG_DECIMAL, true); return 0; }

uint8_t CPU::LDA() { A = fetched; setFlag(FLAG_ZERO, A == 0); setFlag(FLAG_NEGATIVE, (A & 0x80) != 0); return 0; }
uint8_t CPU::LDX() { X = fetched; setFlag(FLAG_ZERO, X == 0); setFlag(FLAG_NEGATIVE, (X & 0x80) != 0); return 0; }
uint8_t CPU::LDY() { Y = fetched; setFlag(FLAG_ZERO, Y == 0); setFlag(FLAG_NEGATIVE, (Y & 0x80) != 0); return 0; }

uint8_t CPU::STA() { writeByte(addr, A); return 0; }
uint8_t CPU::STX() { writeByte(addr, X); return 0; }
uint8_t CPU::STY() { writeByte(addr, Y); return 0; }

uint8_t CPU::NOP() { return 0; }

uint8_t CPU::BRK() {
    PC++;
    writeByte(0x0100 + SP--, (PC >> 8) & 0xFF);
    writeByte(0x0100 + SP--, PC & 0xFF);
    writeByte(0x0100 + SP--, status | FLAG_BREAK | FLAG_UNUSED);
    setFlag(FLAG_INTERRUPT, true);
    uint16_t lo = readByte(0xFFFE);
    uint16_t hi = readByte(0xFFFF);
    PC = (hi << 8) | lo;
    return 0;
}

// Helper: combine ROR behavior
uint8_t CPU::ROR_Helper(uint8_t value) {
    uint8_t carryIn = getFlag(FLAG_CARRY) ? 0x80 : 0;
    uint8_t result = (value >> 1) | carryIn;
    setFlag(FLAG_CARRY, (value & 0x01) != 0);
    setFlag(FLAG_ZERO, result == 0);
    setFlag(FLAG_NEGATIVE, (result & 0x80) != 0);
    return result;
}

////////////////////////////////
// Illegal/unimplemented opcodes
////////////////////////////////
 
// —— Single-byte aliases
uint8_t CPU::ARR() { // AND then ROR
    A &= fetched;
    A = ROR_Helper(A);
    // Overflow = bit6 ^ bit5 after ROR
    bool bit5 = A & (1 << 5);
    bool bit6 = A & (1 << 6);
    setFlag(FLAG_OVERFLOW, bit6 ^ bit5);
    return 0;
}

uint8_t CPU::ASR() { // AND then LSR
    A &= fetched;
    bool carry = A & 1;
    A >>= 1;
    setFlag(FLAG_CARRY, carry);
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, false);
    return 0;
}

uint8_t CPU::ATX() { // AND then TAX
    A &= fetched;
    X = A;
    setFlag(FLAG_ZERO, X == 0);
    setFlag(FLAG_NEGATIVE, X & 0x80);
    return 0;
}

uint8_t CPU::AXS() { // AND then SBX (A-X)
    A &= fetched;
    uint8_t result = A - X;
    setFlag(FLAG_CARRY, A >= X);
    setFlag(FLAG_ZERO, result == 0);
    setFlag(FLAG_NEGATIVE, result & 0x80);
    X = result;
    return 0;
}

uint8_t CPU::ISC() { // same as INS/ISB: INC then SBC
    // implementation relies on standard INC and SBC
    // perform INC
    uint8_t val = fetched + 1;
    writeByte(addr, val);
    setFlag(FLAG_ZERO, val == 0);
    setFlag(FLAG_NEGATIVE, val & 0x80);
    // fetch new
    fetched = val;
    // perform SBC
    return SBC();
}

uint8_t CPU::DCP() { // DEC then CMP
    uint8_t val = fetched - 1;
    writeByte(addr, val);
    setFlag(FLAG_ZERO, (A - val) == 0);
    setFlag(FLAG_CARRY, A >= val);
    setFlag(FLAG_NEGATIVE, ((A - val) & 0x80) != 0);
    return 0;
}

uint8_t CPU::SLO() { // ASL then ORA
    uint8_t value = fetched;
    bool carry = value & 0x80;
    uint8_t shifted = value << 1;
    writeByte(addr, shifted);
    setFlag(FLAG_CARRY, carry);
    A |= shifted;
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    return 0;
}

uint8_t CPU::RLA() { // ROL then AND
    // perform ROL
    uint8_t old = fetched;
    uint8_t carryIn = getFlag(FLAG_CARRY) ? 1 : 0;
    uint8_t result = (old << 1) | carryIn;
    setFlag(FLAG_CARRY, old & 0x80);
    writeByte(addr, result);
    fetched = result;
    // AND with A
    A &= fetched;
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    return 0;
}

uint8_t CPU::SRE() { // LSR then EOR
    uint8_t value = fetched;
    bool carry = value & 0x01;
    uint8_t shifted = value >> 1;
    writeByte(addr, shifted);
    setFlag(FLAG_CARRY, carry);
    A ^= shifted;
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    return 0;
}

uint8_t CPU::RRA() { // ROR then ADC
    // perform ROR
    uint8_t old = fetched;
    uint8_t result = ROR_Helper(old);
    writeByte(addr, result);
    fetched = result;
    // then ADC
    return ADC();
}

uint8_t CPU::LAX() { // LDA then LDX
    A = fetched;
    X = A;
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    return 0;
}

uint8_t CPU::SAX() { // store A & X
    writeByte(addr, A & X);
    return 0;
}

// AXA (aka AHX): write (A & X) & ((high-byte(addr) + 1) & 0xFF)
uint8_t CPU::AXA() {
    uint8_t v = A & X;
    uint8_t hi = uint8_t((addr >> 8) + 1);
    writeByte(addr, v & hi);
    return 0;
}

// XAS (aka SHY/SHX): same masking behavior, but used on ABS,X
uint8_t CPU::XAS() {
    uint8_t v = A & X;
    uint8_t hi = uint8_t((addr >> 8) + 1);
    writeByte(addr, v & hi);
    return 0;
}

uint8_t CPU::SKB() {
    // ‘addr’ has already been computed by addr_ABY() and fetched loaded.
    // We just drop the value.
    (void)fetched;
    return 0;
}

uint8_t CPU::LAR() { // AND then transfer to A,X,SP
    uint8_t value = fetched & SP;
    SP = value;
    A = value;
    X = value;
    setFlag(FLAG_ZERO, value == 0);
    setFlag(FLAG_NEGATIVE, value & 0x80);
    return 0;
}

uint8_t CPU::XAA() { // speculative: transfer A to X then AND fetched
    X = A;
    A &= fetched;
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    return 0;
}

uint8_t CPU::ANE() { // AND then EOR transfer to A,X
    A &= fetched;
    X = A;
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    return 0;
}

uint8_t CPU::TOP() { // triple‐NOP: just consume extra cycle
    return 0;
}

uint8_t CPU::DOP() { // double‐NOP
    return 0;
}

uint8_t CPU::SXA() { // store X & A with high bit
    uint16_t t = (addr & 0xFF00) | (A & X);
    writeByte(t, X);
    return 0;
}

uint8_t CPU::SYA() {
    uint16_t t = (addr & 0xFF00) | (A & Y);
    writeByte(t, Y);
    return 0;
}

uint8_t CPU::ANC() {
    A &= fetched;
    setFlag(FLAG_CARRY, (A & 0x80) != 0);
    setFlag(FLAG_ZERO, A == 0);
    setFlag(FLAG_NEGATIVE, A & 0x80);
    return 0;
}

// Illegal opcode handler
uint8_t CPU::ILL() {
    std::cerr << "Illegal opcode 0x" << std::hex << (int)opcode
        << " at PC=0x" << PC - 1 << std::dec << "\n";
    return 0;
}