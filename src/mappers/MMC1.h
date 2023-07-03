#ifndef MMC1_h
#define MMC1_h

#include "../cart.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct {
	CART *cart;
	FILE *fp; // Battery file. RAM is stored in the following sequence: PRG RAM, PRG NVRAM, CHR RAM, CHR NVRAM
	uint8_t shift_register;

	uint8_t control;
	uint8_t chr_bank_0;
	uint8_t chr_bank_1;
	uint8_t prg_bank;
} MMC1_ctx;


// Returns a heap-allocated string consisting of all characters from 'subj' that appear
// after the last appearance of char 'tgt', or NULL if 'tgt' was not found or was the last character.
// Uses strlen(), so will segfault if 'subj' is not null-terminated.
static char* strip_before(const char *subj, char tgt){
	const size_t old_len = strlen(subj) + 1;

	if(old_len < 3 || subj[old_len - 2] == tgt){
		return NULL;
	}

	int index_of = -1;
	for(int i = old_len - 2; i > -1; i--){
		if(subj[i] == tgt){
			index_of = i + 1;
			break;
		}
	}

	if(index_of == -1){
		return NULL;
	}

	size_t new_len = old_len - index_of;
	char *out = (char*)malloc(sizeof(char) * new_len);
	for(size_t i = 0; i < new_len; i++){
		out[i] = subj[index_of + i];
	}
	out[new_len - 1] = '\0'; 

	return out;
}

MMC1_ctx *MMC1_new_ctx(CART *cart, const char *filename){
	MMC1_ctx *ctx = (MMC1_ctx*)malloc(sizeof(MMC1_ctx));
	ctx->cart = cart;
	
	// Check for PRG RAM. If size != 0 AND has_PRG_RAM then open a .sav file.
	if(cart->has_PRG_RAM && cart->PRG_RAM_size != 0){

		char *fn = strip_before(filename, '/');
		if(fn == NULL){
			fn = (char*)malloc((strlen(filename) + 1)  * sizeof(char));
			strncpy(fn, filename, strlen(filename) + 1);
		} else {
			printf("Stripped last slash from string, is now %s.\n", fn);
		}

		size_t len = strlen(fn) + 1;

		int index_of = -1;
		for(int i = len - 1; i > -1; i--){
			if(fn[i] == '.'){
				index_of = i;
				break;
			}
		}

		const char *new_extn = "sav";

		if(index_of == -1){
			len += 4;
			fn = (char*)realloc(fn, len);
			fn[len - 5] = '.';
		} else if((unsigned)index_of != len - 5) {
			len = index_of + 5;
			fn = (char*)realloc(fn, len);
		}

		for(size_t i = 0; i < 4; i++){
			fn[len - 4 + i] = new_extn[i];
		}

		printf("Will save battery to %s\n", fn);
		ctx->fp = fopen(fn, "r+");
		free(fn);
	} else {
		ctx->fp = NULL;
	}

	ctx->shift_register = 0;
	ctx->control = 0;
	ctx->chr_bank_0 = 0;
	ctx->chr_bank_1 = 0;
	ctx->prg_bank = 0;

	return ctx;
}

// PRG
void MMC1_cart_cpu_write(uint16_t address, uint8_t value, MMC1_ctx *ctx){
	if(0x6000 <= address && address <= 0x7FFF){
		// PRG RAM. If this is present in the cart (aka our file pointer isn't null), mod it by the size if NES2, then work out which bank it's using and write to the appropriate address.
		if(ctx->fp != NULL){
			fseek(ctx->fp, address-0x6000, SEEK_SET);
			fputc(value, ctx->fp);
			rewind(ctx->fp); // TODO unnecessary, as fseek seeks from beginning/SEEK_SET.
		}
	} else if(0x8000 <= address){
		// ...oh boy. This is a write to the 'shift' register, which the NES needs to use to control banking. It basically writes
		// individual bits into our shift register before writing a final one with a particular address and a set 7th bit, selecting the destination // internal register to write to. 
		
		// Additionally since this register is a serial port, it doesn't *actually* read a high bit, rather a rising edge on bit zero. That means that if the serial port is written to consecutively with bit 0 set, it will be ignored due to the lack of rising edge. The reset bit however is a on/off signal, or something else that is not ignored consecutively.
		if((value & 0x80) == 0x80){
			// Reset. Depending on the address, we now write the shift register to the desired internal register and reset it.
			if(0x8000 <= address && address <= 0x9FFF){
				// Control, write 5 bits.
				ctx->control = (ctx->shift_register & 0x1F);
			} else if(0xA000 <= address && address <= 0xBFFF){
				// CHR bank 0
				ctx->chr_bank_0 = (ctx->shift_register & 0x1F);
			} else if(0xC000 <= address && address <= 0xDFFF){
				// CHR bank 1
				ctx->chr_bank_1 = (ctx->shift_register & 0x1F);
			} else if(0xE000 <= address){
				// PRG bank
				ctx->prg_bank = (ctx->shift_register & 0x1F);
			}
			ctx->shift_register = 0;
		} else {
			ctx->shift_register <<= 1;
			ctx->shift_register |= (value & 1);
		}

	}
}

uint8_t MMC1_cart_cpu_read(uint16_t address, MMC1_ctx *ctx){
	// Depending on the address, this has to go to different parts of the cartridge.
	if(address < 0x6000){
		printf("Warning: Attempted read from MMC1 cart from unmapped address 0x%04X. Returning 0xFF.\n", address);
		return 0xFF;
	} else if(0x6000 <= address && address <= 0x7FFF){
		// Read to PRG RAM. If it's not NULL, read from it, else return 0xFF. TODO what does the actual NES return here?
		if(ctx->fp != NULL){
			fseek(ctx->fp, address-0x6000, SEEK_SET);
			return fgetc(ctx->fp);
		} else {
			return 0xFF;
		}
	} else if(0x8000 <= address && address <= 0xBFFF){
		// First PRG ROM bank. Each ROM bank is 16KiB in size, and on some MMC1s the first bank is switchable.
		// To work that out, we need to know which banking mode we're in, which bits 2 and 3 of control tell us:
		// (The below values are the result of evaluating (control >> 2) & 3).
		// 0,1 - 32KiB bank is mapped to both this bank and the next bank. 32KiB offset determined by {PRG bank reg} & 0xFE.
		// 2   - First bank locked to this address range, bank number switches bank starting at 0xC000
		// 3   - Last bank locked to next address range, bank number switches bank starting at 0x8000.

		size_t prg_address = 16;
		if(ctx->cart->trainer_present){
			prg_address += 512;
		}

		switch((ctx->control >> 2) & 0x3){
			case 0:
			case 1:
				// 32KiB mode, so add {PRG bank} & 0xFE * 0x8000 to {address - 0x8000} and return whatever byte is there in the cart.
				{
					size_t new_address = (ctx->prg_bank & 0xE) * 0x8000;
					new_address += address - 0x8000;
					new_address %= ctx->cart->PRG_ROM_len * 0x4000;

					prg_address += new_address;
				}
				break;
			case 2:
				// We're locked to the first bank.
				prg_address += address - 0x8000;
				break;
			case 3:
				// Last bank is locked, we're using the PRG number.
				{
					size_t new_address = (ctx->prg_bank & 0xF) * 0x4000;
					new_address += address - 0x8000;
					new_address %= ctx->cart->PRG_ROM_len * 0x4000;
					prg_address += new_address;
				}
				break;
		}

		// TODO remove
		assert(prg_address < ctx->cart->filesize);
		return ctx->cart->ROM_contents[prg_address];
	} else if(0xC000 <= address){
		// Second PRG ROM bank. This is as above, though cases 2 and 3 are swapped, the 'lock' case has different behaviour
		// (locks to last 0x4000 bytes instead of first) and we won't modify cases 0 and 1 since the extra 0x4000 will
		// serve as the 16KiB 'offset'.

		size_t prg_address = 16;
		if(ctx->cart->trainer_present){
			prg_address += 512;
		}

		switch((ctx->control >> 2) & 0x3){
			case 0:
			case 1:
				{
					size_t new_address = (ctx->prg_bank & 0xE) * 0x8000;
					new_address += address - 0x8000;
					new_address %= ctx->cart->PRG_ROM_len * 0x4000;

					prg_address += new_address;
				}
				break;
			case 2:
				{
					size_t new_address = (ctx->prg_bank & 0xF) * 0x4000;
					new_address += address - 0xC000;
					new_address %= ctx->cart->PRG_ROM_len * 0x4000;

					prg_address += new_address;
				}
				break;
			case 3:
				{
					prg_address += (address - 0xC000) + ((ctx->cart->PRG_ROM_len-1) * 0x4000);
				}
				break;
		}
		
		// TODO remove
		assert(prg_address < ctx->cart->filesize);
		return ctx->cart->ROM_contents[prg_address];
	}

	// Satiate compiler.
	return 0;
}

// CHR TODO
void MMC1_cart_gpu_write(uint16_t address, uint8_t value, MMC1_ctx *ctx){
	(void)address;
	(void)value;
	(void)ctx;
	return;
}

uint8_t MMC1_cart_gpu_read(uint16_t address, MMC1_ctx *ctx){
	(void)address;
	(void)ctx;
	return 0xFF;
}

// This will destroy the MMC1 struct, but won't destroy the cartridge, which must be destroyed separately.
void MMC1_destroy(MMC1_ctx *ctx){
	if(ctx->fp != NULL){
		fclose(ctx->fp);
	}

	free(ctx);
}

#endif
