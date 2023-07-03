// cpu.h
// Written by Matt598, 2023.
//
//	- Definitions for the NES' CPU.
#ifndef cpu_h
#define cpu_h

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "mmu.h"

typedef struct {
	uint8_t A; // Accumulator
	uint8_t X,Y; // Index registers
	uint8_t F; // Flag register
	uint8_t SP; // Stack pointer
	uint16_t PC; // Program counter
	
	MMU* mmu;
	unsigned wait_cycles;
} CPU;

CPU* new_cpu(MMU *mmu){
	CPU* cpu = (CPU*)malloc(sizeof(CPU));
	// TODO is this the best way of doing this?
	memset(cpu, 0, sizeof(CPU));
	cpu->mmu = mmu;
	return cpu;
}

/* This is here for reference.
 *	FLAG REGISTER:
 *		7  6  5  4  3  2  1  0
 *		----------------------
 *		N  V  -  B  D  I  Z  C
 *
 *	N - Negative
 *	V - Overflow
 *	- - Unused
 *	B - Break
 *	D - Decimal (Also unused in the 2A03)
 *	I - Interrupt/IRQ disable
 *	Z - Zero
 *	C - Carry
 */

// Helper functions for addressing
uint8_t zpg_read(CPU *cpu){
	uint16_t addr = mmu_read(cpu->PC++, cpu->mmu);
	return mmu_read(addr, cpu->mmu);
}

uint8_t zpg_read_offset(CPU *cpu, uint8_t offset){
	// Used for indexed zero page reads.
	uint16_t addr = mmu_read(cpu->PC++, cpu->mmu);
	addr += offset;
	return mmu_read(addr, cpu->mmu);
}

// BEGIN OPCODE DEFINITIONS
// These are all opcode functions for the CPU, which may take inputs pending their type.
// Since the inputs themselves determine the number of cycles it takes, we'll do that in
// the switch statement and just generate them here.

// Control flow functions
void JMP(CPU *cpu, bool is_absolute){
	// Jump to a location in memory, done weirdly depending on which mode.
	union {
		uint8_t bytes[2];
		uint16_t addr;
	} addr;

	if(is_absolute){
		// Two bytes following PC are the new PC.
		addr.bytes[0] = mmu_read(cpu->PC++, cpu->mmu);
		addr.bytes[1] = mmu_read(cpu->PC++, cpu->mmu);
		cpu->PC = (uint16_t)addr.addr; // ah yes, endianess conversion.
	} else {
		// Two bytes following PC are the address of the LSB (first byte, since the 2A03 is LE)
		// that we're jumping to.
		addr.bytes[0] = mmu_read(cpu->PC++, cpu->mmu);
		addr.bytes[1] = mmu_read(cpu->PC++, cpu->mmu);

		uint16_t location = (uint16_t)addr.addr;

		addr.bytes[0] = mmu_read(location++, cpu->mmu);
		addr.bytes[1] = mmu_read(location++, cpu->mmu);

		cpu->PC = (uint16_t)addr.addr;	
	}
}

// Miscellaneous Control Functions

void SEI(CPU *cpu){
	// Set interrupt disable - turns interrupts off.
	cpu->F |= 4;
}

void CLD(CPU *cpu){
	// Clear the decimal flag. This does nothing, since the 2A03 doesn't support BCD mode.
	cpu->F &= 0xF7;
}
// RMW functions
void STA(CPU *cpu, uint16_t address){
	// Store accumulator.
	mmu_write(address, cpu->A, cpu->mmu);
}

void LDA(CPU *cpu, uint8_t value){
	// Load into accumulator. Modifies negative and zero.
	cpu->A = value;
	cpu->F &= (0x7F | (cpu->A & 0x80));
	cpu->F &= (0xFD | (cpu->A ? 0 : 0x2));
}

void STX(CPU *cpu, uint16_t address){
	// Store X.
	mmu_write(address, cpu->X, cpu->mmu);
}

void LDX(CPU *cpu, uint8_t value){
	cpu->X = value;
	cpu->F &= (0x7F | (cpu->X & 0x80));
	cpu->F &= (0xFD | (cpu->X ? 0 : 0x2));
}

void TXS(CPU *cpu){
	// Transfer X into S (stack register).
	cpu->SP = cpu->X;
}

// ALU functions
void EOR(CPU *cpu, uint8_t value){
	// E-xclusive OR with accumulator.
	cpu->A ^= value;
	cpu->F &= (0x7F | (cpu->A & 0x80));
	cpu->F &= (0xFD | (cpu->A ? 0 : 0x2));
}

void ORA(CPU *cpu, uint8_t value){
	// (inclusive) OR with accumulator.
	cpu->A |= value;
	cpu->F &= (0x7F | (cpu->A & 0x80));
	cpu->F &= (0xFD | (cpu->A ? 0 : 0x2));
}
// END OPCODE DEFINITIONS


void tick_cpu(CPU *cpu){
	/* The NES' ISA separates instruction into 4 'groups' based on their two bottom bits:
		- 0b00
			- Control instructions
		- 0b01
			- ALU instructions
		- 0b10
			- RMW (read,modify,write) instructions
		- 0b11
			- Combination ALU/RMW, e.g. rotating a value at a 0-page memory address.

	  Additionally, the CPU uses memory addresses 0x0000-0x00FF as the 'zero page' - allowing them
	  to be addressed with an 8-bit input, and thereby being much faster then using a full 16-bit address.

	  Finally, unlike other CPUs, illegal opcodes in the NES' CPU aren't HCF. Instead, they act similarly to their
	  adjacent instructions, or are just NOPs. Since certain late games actually make use of these, we need to implement
	  the entire table.
	*/

	// Fetch
	uint8_t inst = mmu_read(cpu->PC++, cpu->mmu);

	// Decode, then execute. The actual family of instructions we have to worry about is fairly small, so most of this is
	// just delegating to functions.
	switch(inst){

		case 0x05:
			ORA(cpu, zpg_read(cpu));
			cpu->wait_cycles = 2;
			break;

		case 0x49:
			EOR(cpu, mmu_read(cpu->PC++, cpu->mmu));
			cpu->wait_cycles = 1;
			break;

		case 0x4C:
			JMP(cpu, true);
			cpu->wait_cycles = 2;
			break;

		case 0x78:
			SEI(cpu);
			cpu->wait_cycles = 1;
			break;

		case 0x85:
			STA(cpu, zpg_read(cpu));
			cpu->wait_cycles = 2;
			break;

		case 0x8D:
			STA(cpu, cpu_read16(cpu->PC++, cpu->mmu->mmc));
			cpu->PC++;
			break;

		case 0x95:
			STA(cpu, zpg_read_offset(cpu, cpu->X));
			cpu->wait_cycles = 3;
			break;

		case 0x99:
			STA(cpu, cpu_read16(cpu->PC++, cpu->mmu->mmc) + cpu->Y);
			cpu->PC++;
			cpu->wait_cycles = 4;
			break;

		case 0x9A:
			TXS(cpu);
			cpu->wait_cycles = 1;
			break;

		case 0x9D:
			STA(cpu, cpu_read16(cpu->PC++, cpu->mmu->mmc) + cpu->X);
			cpu->PC++;
			cpu->wait_cycles = 4;
			break;
		
		case 0xA2:
			LDX(cpu, cpu_read(cpu->PC++, cpu->mmu->mmc));
			cpu->wait_cycles = 1;
			break;

		case 0xA5:
			LDA(cpu, zpg_read(cpu));
			cpu->wait_cycles = 2;
			break;

		case 0xA6:
			LDX(cpu, zpg_read(cpu));
			cpu->wait_cycles = 2;
			break;

		case 0xA9:
			LDA(cpu, mmu_read(cpu->PC++, cpu->mmu));
			cpu->wait_cycles = 1;
			break;

		case 0xAD:
			LDA(cpu, mmu_read(cpu_read16(cpu->PC++, cpu->mmu->mmc), cpu->mmu));
			cpu->PC++;
			cpu->wait_cycles = 3;
			break;

		case 0xB5:
			LDA(cpu, zpg_read_offset(cpu, cpu->X));
			cpu->wait_cycles = 3;
			break;

		case 0xB6:
			LDX(cpu, zpg_read_offset(cpu, cpu->Y));
			cpu->wait_cycles = 3;
			break;

		case 0xB9:
			// TODO wait_cycles needs to be 4 if the read crosses a page
			LDA(cpu, mmu_read(cpu_read16(cpu->PC++, cpu->mmu->mmc) + cpu->Y, cpu->mmu));
			cpu->PC++;
			cpu->wait_cycles = 3;
			break;

		case 0xBD:
			// TODO wait_cycles needs to be 4 if the read crosses a page
			LDA(cpu, mmu_read(cpu_read16(cpu->PC++, cpu->mmu->mmc) + cpu->X, cpu->mmu));
			cpu->PC++;
			cpu->wait_cycles = 3;
			break;

		case 0xD8:
			CLD(cpu);
			cpu->wait_cycles = 1;
			break;

		default:
			printf("Unknown opcode encountered!\n\tAddress: 0x%04X\n\topcode: 0x%02X\n\ttwo bytes following opcode: 0x%02X 0x%02X\n", cpu->PC, inst, mmu_read(cpu->PC, cpu->mmu), mmu_read(cpu->PC+1, cpu->mmu));
			abort();
	}
}



#endif
