#ifndef cart_h
#define cart_h

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
// FIXME platform-dependent file I/O
#include <unistd.h>

enum ROM_types {
	iNES,
	oldiNES,
	iNES07,
	NES2
};

// TODO collapse this into the extended console type variable, since 0-2 in that map to the same values here.
enum system_types {
	NESFAMI,
	NINVS,
	NINPS10,
	EXTN
};

enum timing_modes {
	RP2C02, // NTSC NES
	RP2C07, // (Licensed) PAL NES
	MULTI,  // Multiple-region
	UA6538  // "Dendy"
};

// TODO support alternate mode of file read/writes instead of caching the entire ROM in memory for low RAM usage (at the cost of significant latency)
typedef struct {
	uint8_t *ROM_contents;
	enum ROM_types type;
	size_t filesize;
	
	// Info. Note that the two ROM_len values here are masked to 0xFF if we're using an iNES rom.	
	uint16_t PRG_ROM_len; // *16,384 (16KiB units)
	uint16_t CHR_ROM_len; // *8,192  (8KiB  units)
	
	uint16_t mapper; // 12-bit
	uint8_t submapper; // Only used in NES2.0
	bool mirroring; // true = 1 = vertical, false = - = horizontal
	bool has_PRG_RAM;
	bool trainer_present;
	bool ignore_mirroring_bit;
	enum system_types sys_type;
	bool NES2_fmt_override;
	uint8_t PRG_RAM_size; // *8,192  (8KiB units) (Assume 1 if 0)
	uint8_t PRG_NVRAM_size; // This and PRG_RAM are 'shift count's, so the actual size is 64 << {PRG_(NV)RAM}.
	uint8_t CHR_RAM_size;
	uint8_t CHR_NVRAM_size; // Same as above.
	enum timing_modes timing_type;
	uint8_t VS_PPU_type;
	uint8_t VS_HW_type;
	uint8_t extended_console_type;
	uint8_t misc_rom_count;
	uint8_t default_expn_device;
	uint8_t TV_system; // 0 = NTSC, 2 = PAL, else dual-compatible (default to NTSC)
	bool bus_conflicts_specified;
	bool PRG_RAM_present;
	bool uncertain_type;
} CART;

CART* new_cart(const char *ROM_image){
	// Try to open and load the image into memory. We won't worry about flags just yet,
	// we'll just load the entire file into memory and then work it out.
	FILE *fp = fopen(ROM_image, "r");

	if(fp == NULL){
		fprintf(stderr, "Fatal: failed to open file. errno = %d\n", errno);
		return NULL;
	}

	fseek(fp, 0, SEEK_END);
	size_t filesize = ftell(fp);
	rewind(fp);

	CART* out = (CART*)malloc(sizeof(CART));
	out->ROM_contents = (uint8_t*)malloc(sizeof(uint8_t)*filesize); // Redundancy!
	out->filesize = filesize;
	size_t read_len = fread(out->ROM_contents, sizeof(uint8_t), filesize, fp);
	if(read_len == 0){
		fprintf(stderr, "Fatal: failed to read file. errno = %d\n", errno);
		fclose(fp);
		free(out->ROM_contents);
		free(out);
		return NULL;
	}

	fclose(fp);
	// Now we get to read the ROM header! The first 4 bytes should be 0x4E 0x45 0x53 0x1A. If not,
	// it's not a valid ROM.
	if(*(uint32_t*)out->ROM_contents != 0x1A53454E){
		fprintf(stderr, "Fatal: ROM is not valid: missing magic number.\n");
		free(out->ROM_contents);
		free(out);
		return NULL;
	}

	// Since the values of the bytes from here-on out have different meanings pending the ROM type,
	// we'll need to check the 7th byte's 8- and 4-bits. If just the 8 bit is set, we're dealing
	// with an NES2.0 ROM.
	out->uncertain_type = false;
	switch(out->ROM_contents[7] & 0x0C){
		case 0x08:
			{
				// Run the check after that's been confirmed. TODO put this into the IF!
				// This just checks if the size of the ROM image overall, that being the size
				// of the ROM banks plus the header, exceeds the size of the file. If it does,
				// we're not reading a NES2.0 ROM, and vice versa.
				uint16_t PRG_ROM_len = out->ROM_contents[4] + ((uint16_t)(out->ROM_contents[9] & 0xF) << 8);
				uint16_t CHR_ROM_len = out->ROM_contents[5] + ((uint16_t)((out->ROM_contents[9]) >> 4) << 8);
		
				size_t filesize_pred = 512 + 8192*CHR_ROM_len + 16384*PRG_ROM_len;
				if(filesize < filesize_pred){
					// iNES
					out->type = iNES;
					out->PRG_ROM_len = out->ROM_contents[4];
					out->CHR_ROM_len = out->ROM_contents[5];
				} else {
					out->type = NES2;
					out->PRG_ROM_len = PRG_ROM_len;
					out->CHR_ROM_len = CHR_ROM_len;
				}
			}
			break;

		case 0x04:
			out->type = oldiNES;
			out->PRG_ROM_len = out->ROM_contents[4];
			out->CHR_ROM_len = out->ROM_contents[5];
			break;

		case 0x00:
			if(*(uint32_t*)(out->ROM_contents + 12) == 0x00){
				out->type = iNES;
				out->PRG_ROM_len = out->ROM_contents[4];
				out->CHR_ROM_len = out->ROM_contents[5];
			} else {
				out->type = oldiNES;
				out->PRG_ROM_len = out->ROM_contents[4];
				out->CHR_ROM_len = out->ROM_contents[5];
				out->uncertain_type = true;
			}
			break;

		default:
			out->type = oldiNES;
			out->PRG_ROM_len = out->ROM_contents[4];
			out->CHR_ROM_len = out->ROM_contents[5];
			out->uncertain_type = true;
			break;
	}

	if(out->uncertain_type){
		printf("Warning: Could not definitively determine ROM format. Errors may occur.\n");
	}

	// Now we know the type of ROM we're dealing with, so we can proceed with the rest of the flags in the header.
	out->mirroring = out->ROM_contents[6] & 1;
	out->has_PRG_RAM = out->ROM_contents[6] & 2;
	out->trainer_present = out->ROM_contents[6] & 4;
	out->ignore_mirroring_bit = out->ROM_contents[6] & 8;

	out->mapper = out->ROM_contents[6] >> 4;

	switch(out->ROM_contents[7] & 3){
		case 0:
			out->sys_type = NESFAMI;
			break;
		case 1:
			out->sys_type = NINVS;
			break;
		case 2:
			out->sys_type = NINPS10;
			break;
		case 3:
			out->sys_type = EXTN;
			break;
	}

	out->NES2_fmt_override = (((out->ROM_contents[7] >> 2) & 3) == 2);
	if(out->NES2_fmt_override){
		out->type = NES2;
		// Run size check again to prevent overrunning.
		uint16_t PRG_ROM_len = out->ROM_contents[4] + ((uint16_t)(out->ROM_contents[9] & 0xF) << 8);
		uint16_t CHR_ROM_len = out->ROM_contents[5] + ((uint16_t)((out->ROM_contents[9]) >> 4) << 8);
		
		size_t filesize_pred = 512 + 8192*CHR_ROM_len + 16384*PRG_ROM_len;

		if(filesize < filesize_pred){
			// Error
			printf("Fatal: NES2 override bit set, but stated ROM size exceeded filesize. ROM is likely corrupt.\n");
			
			free(out->ROM_contents);
			free(out);
			return NULL;
		} else {
			out->type = NES2;
			out->PRG_ROM_len = PRG_ROM_len;
			out->CHR_ROM_len = CHR_ROM_len;
		}
	}

	out->mapper += out->ROM_contents[7] & 0xF0;
	
	// From here, NES2.0 and iNES use bytes 8-15 for different things, so we'll branch off here.
	if(out->type == NES2){
		out->submapper = out->ROM_contents[8] >> 4;
		out->mapper += ((uint16_t)out->ROM_contents[8] & 0xF) << 8;

		// We already took care of byte 9 when we switched to NES2.0 format, so we'll skip it here.
		
		out->PRG_RAM_size = out->ROM_contents[10] & 0xF;
		out->PRG_NVRAM_size = out->ROM_contents[10] >> 4;
		out->CHR_RAM_size = out->ROM_contents[11] & 0xF;
		out->CHR_NVRAM_size = out->ROM_contents[11] >> 4;
	
		switch(out->ROM_contents[12] & 3){
			case 0:
				out->timing_type = RP2C02;
				break;
			case 1:
				out->timing_type = RP2C07;
				break;
			case 2:
				out->timing_type = MULTI;
				break;
			case 3:
				out->timing_type = UA6538;
				break;
		}

		if(out->sys_type == NINVS){
			out->VS_PPU_type = out->ROM_contents[13] & 0xF;
			out->VS_HW_type = out->ROM_contents[13] >> 4;
		} else if(out->sys_type == EXTN){
			out->extended_console_type = out->ROM_contents[14] & 0xF;	
		}

		// misc_rom_count default_expn_device
		out->misc_rom_count = out->ROM_contents[14] & 3;
		out->default_expn_device = out->ROM_contents[15] & 0x3F;
	} else {
		out->PRG_RAM_size = out->ROM_contents[8] == 0 ? 1 : out->ROM_contents[8];

		out->timing_type = out->ROM_contents[9] & 1 ? RP2C07 : RP2C02; // TODO should this be here, or should we assume NTSC for iNES ROMs?

		switch(out->ROM_contents[10] & 3){
			case 0:
				if(out->timing_type != RP2C02){
					printf("Warning: 9th byte of ROM header specified PAL, but 10th byte specified NTSC. Defaulting to NTSC. Use the '--override-tv-format' flag if the ROM behaves strangely.\n");
					out->timing_type = RP2C02;
				}
				break;
			case 2:
				if(out->timing_type != RP2C07){
					printf("Warning: 9th byte of ROM header specified NTSC, but 10th byte specified PAL. Defaulting to NTSC. Use the '--override-tv-format' flag if the ROM behaves strangely.\n");
				}
				out->timing_type = RP2C07;
				break;
			default:
				printf("Warning: NTSC/PAL cross-compatible ROM found, defaulting to NTSC. Use the '--override-tv-format' flag if the ROM behaves strangely.\n");
				out->timing_type = RP2C02;
		}

		out->mapper &= 0xFF;
	}

	return out;
}

void destroy_cart(CART *cart){
	free(cart->ROM_contents);
	free(cart);
}

void print_cart_info(CART *cart){
	char *fmt_str = "N/A";
	uint8_t fmt = cart->type;
	fmt += cart->uncertain_type ? 4 : 0;
	switch(fmt){
		case 0:
			fmt_str = "iNES";
			break;
		case 1:
			fmt_str = "Archaic iNES";
			break;
		case 2:
			fmt_str = "iNES 0.7";
			break;
		case 3:
			fmt_str = "NES 2.0";
			break;
		case 4:
			fmt_str = "iNES (Uncertain)";
			break;
		case 5:
			fmt_str = "Archaic iNES (Uncertain)";
			break;
		case 6:
			fmt_str = "iNES 0.7 (Uncertain)";
			break;
		case 7:
			fmt_str = "iNES 2.0 (Uncertain)";
			break;
	}

	char *sys_type_str = "N/A";
	switch(cart->sys_type){
		case NESFAMI:
			sys_type_str = "Nintendo Entertainment System or Nintendo Famicom";
			break;
		case NINVS:
			sys_type_str = "Nintendo Vs. UniSystem or Nintendo Vs. DualSystem";
			break;
		case NINPS10:
			sys_type_str = "Nintendo Playchoice 10";
			break;
		case EXTN:
			switch(cart->extended_console_type & 0xF){
				case 0x0:
					sys_type_str = "Nintendo Entertainment System, Nintendo Famicom or Dendy (Extended)";
					break;
				case 0x1:
					sys_type_str = "Nintendo Vs. UniSystem or Nintendo Vs. DualSystem";
					break;
				case 0x2:
					sys_type_str = "Nintendo Playchoice 10";
					break;
				case 0x3:
					sys_type_str = "Nintendo Famicom clone with 6502-compatible CPU";
					break;
				case 0x4:
					sys_type_str = "Nintendo Entertainment System or Nintendo Famicom with EPSM/Plug-through cartridge";
					break;
				case 0x5:
					sys_type_str = "V.R. Technology VT01 with red/cyan STN palette";
					break;
				case 0x6:
					sys_type_str = "V.R. Technology VT02";
					break;
				case 0x7:
					sys_type_str = "V.R. Technology VT03";
					break;
				case 0x8:
					sys_type_str = "V.R. Technology VT09";
					break;
				case 0x9:
					sys_type_str = "V.R. Technology VT32";
					break;
				case 0xA:
					sys_type_str = "V.R. Technology VT369";
					break;
				case 0xB:
					sys_type_str = "UMC UM6578";
					break;
				case 0xC:
					sys_type_str = "Nintendo Famicom Network System";
					break;
				case 0xD:
				case 0xE:
				case 0xF:
					sys_type_str = "Unknown (Reserved)";
					break;
			}
	}

	char *timing_md = "N/A";
	switch(cart->timing_type){
		case RP2C02:
			timing_md = "RP2C02 (NTSC)";
			break;
		case RP2C07:
			timing_md = "RP2C07 (PAL)";
			break;
		case MULTI:
			timing_md = "Dual-compatible (NTSC/PAL)";
			break;
		case UA6538:
			timing_md = "UA6538 (Dendy)";
			break;
	}
	
	double prg_ram_len;

	if(cart->type == NES2){
		prg_ram_len = (64 << cart->PRG_RAM_size) / 1024.0F;
	} else {
		prg_ram_len = 8192*cart->PRG_RAM_size;
	}



	printf("=== BEGIN ROM INFO ===\n");
	printf( 
		"File info:\n"
		"\tFile size: %fKiB\n"
		"\tROM format: %s\n" 
		"\tNES 2.0 Identifier present: %s\n"
		"ROM info:\n"
		"\tPRG ROM Size: %dKiB\n"
		"\tCHR ROM Size: %dKiB\n"
		"\tMapper number: 0x%04X\n"
		"\tSubmapper number (ignore if not NES 2.0): 0x%02X\n"
		"\tMirroring: %s\n"
		"\tPRG RAM Present: %s\n"
		"\tTrainer Present: %s\n"
		"\tForce four screen vram: %s\n"
		"\tSystem Type: %s\n"
		"\tPRG RAM Size: %fKiB\n"
		"\tPRG NVRAM Size: %fKiB\n"
		"\tCHR RAM Size: %fKiB\n"
		"\tCHR NVRAM Size: %fKiB\n"
		"\tTiming mode: %s\n"
		"===  END ROM INFO  ===\n",
		cart->filesize/1024.0F, fmt_str, cart->NES2_fmt_override ? "Yes" : "No",
		cart->PRG_ROM_len*16, cart->CHR_ROM_len*8, cart->mapper, cart->submapper,
		cart->mirroring ? "1 (Vertical)" : "0 (Horizontal)", cart->has_PRG_RAM ? "Yes" : "No",
		cart->trainer_present ? "Yes" : "No", cart->ignore_mirroring_bit ? "Yes" : "No", sys_type_str,
		prg_ram_len, (64 << cart->PRG_NVRAM_size) / 1024.0F, (64 << cart->CHR_RAM_size) / 1024.0F,
		(64 << cart->CHR_NVRAM_size) / 1024.0F, timing_md
	);
}

#endif
