#ifndef mmu_h
#define mmu_h

// This serves as a final delegator for memory reads/writes. It's not an actual simulation of an MMU,
// per se, as it does no access checking, but it will delegate read and write requests to the correct
// area from the address. It also handles memory management for RAM, which is being done on the heap
// to save memory.

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "mappers/delegator.h"
#include "cart.h"

typedef struct {
	uint8_t *ram;
	MMC *mmc;
} MMU;

MMU new_mmu(MMC* mmc){
	MMU mmu;
	mmu.ram = (uint8_t*)malloc(sizeof(uint8_t)*0x800); // Yes, the sizeof() is redundant, but it makes it consistent with the rest of the malloc() calls in this program.
	mmu.mmc = mmc;
	return mmu;
}

uint8_t mmu_read(uint16_t address, MMU *mmu){
	// RAM echoes itself in memory three times after its actual 2KiB block.
	// Again, I am aware that half of these conditions (the left side) are useless,
	// since they are already false given the previous condition's failure. They're
	// kept here for code clarity so one can tell where the read/write is going without having
	// to parse the simplified conditions.
	if(address <= 0x1FFF){
		return mmu->ram[address % 0x800];
	} else if(0x2000 <= address && address <= 0x3FFF){
		// Not implemented TODO
		// These are the 8 PPU registers. Why Nintendo decided to occupy 8KiB for 8 byte-sized registers is beyond me, but it's easy to implement.
		printf("Warning: read attempted at address 0x%04X, PPU registers are not implemented yet! Returning 0xFF.\n", address);
		return 0xFF;
	} else if(0x4000 <= address && address <= 0x4017){
		// Not implemented TODO
		printf("Warning: read attempted at address 0x%04X, APU/IO registers are not implemented yet! Returning 0xFF.\n", address);
		return 0xFF;
	} else if(0x4018 <= address && address <= 0x401F){
		printf("Warning: read attempted at address 0x%04X, CPU Test Mode not supported. Returning 0xFF.\n", address);
		return 0xFF;
	} else {
		// Cartridge space.
		return cpu_read(address, mmu->mmc);
	}
}

void mmu_write(uint16_t address, uint8_t value, MMU *mmu){
	if(address <= 0x1FFF){
		mmu->ram[address % 0x800] = value;
		return;
	} else if(0x2000 <= address && address <= 0x3FFF){
		// Not implemented TODO
		printf("Warning: write attempted at address 0x%04X, PPU registers are not implemented yet! Returning 0xFF.\n", address);
		return;
	} else if(0x4000 <= address && address <= 0x4017){
		// Not implemented TODO
		printf("Warning: write attempted at address 0x%04X, APU/IO registers are not implemented yet! Returning 0xFF.\n", address);
		return;
	} else if(0x4018 <= address && address <= 0x401F){
		printf("Warning: write attempted at address 0x%04X, CPU Test Mode not supported. Returning 0xFF.\n", address);
		return;
	} else {
		// Cartridge space.
		cpu_write(address, value, mmu->mmc->ctx);
		return;
	}
}

// Does not destroy/free MMC.
void destroy_mmu(MMU *mmu){
	free(mmu->ram);
}


#endif
