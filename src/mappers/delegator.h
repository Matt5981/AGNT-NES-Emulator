#ifndef delegator_h
#define delegator_h

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "MMC1.h"

// This is a solution to a situation warranting polymorphism in a language with no polymorphism.
// Since each MMC has different behaviour, memory maps, hardware, etc., we need to call different
// functions depending on which MMC we have - which is what this header is responsible for.
enum MMC_TYPES {
	MMC1,
};

typedef struct {
	void *ctx; // MMC context struct.
	enum MMC_TYPES type;
} MMC;

MMC new_MMC(CART* cart, const char *filename){
	MMC mmc;
	// Switch on the mapper number to return the correct struct.
	switch(cart->mapper){
		case 1:
			// MMC1
			mmc.ctx = MMC1_new_ctx(cart, filename);
			mmc.type = MMC1;
			break;
		default:
			fprintf(stderr, "Fatal: unsupported mapper found (number 0x%04X). Exiting to prevent erroneous behaviour.\n", cart->mapper);
			abort();
	}

	return mmc;
}

void destroy_mmc(MMC *mmc){
	switch(mmc->type){
		case MMC1:
			MMC1_destroy((MMC1_ctx*)mmc->ctx);
			break;
	}
}

uint8_t cpu_read(uint16_t address, MMC *mmc){
	// Apparently returning out of a switch case is "bad practice".

	uint8_t ret = 0;
	switch(mmc->type){
		case MMC1:
			ret = MMC1_cart_cpu_read(address, (MMC1_ctx*)mmc->ctx);
			break;
	}

	return ret;
}

void cpu_write(uint16_t address, uint8_t value, MMC *mmc){
	switch(mmc->type){
		case MMC1:
			MMC1_cart_cpu_write(address, value, (MMC1_ctx*)mmc->ctx);
			break;
	}
	return;
}

// This is used in 2 places exactly: either to read the reset vector when resetting/starting
// or when reading the address for an indirectly-addressed JMP.
uint16_t cpu_read16(uint16_t address, MMC *mmc){
	union {
		uint16_t val;
		uint8_t components[2];
	} conv;

	switch(mmc->type){
		case MMC1:
			conv.components[0] = MMC1_cart_cpu_read(address, (MMC1_ctx*)mmc->ctx);
			conv.components[1] = MMC1_cart_cpu_read(address + 1, (MMC1_ctx*)mmc->ctx);
	}

	return conv.val;
}

#endif
