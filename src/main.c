#include "cpu.h"
#include "cart.h"
#include "mappers/delegator.h"
#include "mmu.h"

#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

bool should_stop = false;

void handle(int signum){
	(void)signum;
	should_stop = true;
}

void print_help_text(){
	printf(
		"Usage:\n"
		"\tAGNT-NES-Emulator {args} {ROM file}\n"
		"Arguments:\n"
		"\t-i, --info\n"
		"\t\tDumps info about the input ROM file and exits.\n"
		"\t--override-tv-format {one of NTSC or PAL}\n"
		"\t\tOverrides the output TV format, between NTSC and PAL. Currently not implemented.\n"
		"\t-f, --force\n"
		"\t\tForces AGNT-NES-Emulator to run the given ROM, regardless of if it supports it or not. This will cause problems!\n"
		"Help:\n"
		"\tAGNT-NES-Emulator will look for battery files with the same name as the input ROM file.\n"
		"\tIf you rename your ROM file, you must rename your battery files to the same name\n"
		"\t(before the file extension) for AGNT-NES-Emulator to find them.\n"
	);
}

int main(int argc, const char *argv[]){
	printf("AGNT NES Emulator v0.1. Programmed by Matt598, 2023.\n");
	if(argc < 2){
		printf("Fatal: No input ROM provided. Use '-h' for help.\n");
		return 1;
	}
	
	// Check for -h or --help in the args since we need to print help text and exit before allocating
	// anything if so.
	signal(SIGINT, handle);
	
	bool cart_info = false;
	bool force_flag = false;

	for(int i = 0; i < argc; i++){
		if(strncmp(argv[i], "-h", 2) == 0 || strncmp(argv[i], "--help", 6) == 0){
			print_help_text();
			return 0;
		} else if(strncmp(argv[i], "-i", 2) == 0 || strncmp(argv[i], "--info", 6) == 0){
			cart_info = true;	
		} else if(strncmp(argv[i], "-f", 2) == 0 || strncmp(argv[i], "--force", 7) == 0){
			force_flag = true;	
		}
	}

	// Try to load cart.
	CART *cart = new_cart(argv[argc-1]);
	if(cart == NULL){
		return 1;
	}

	// If info run, print and return.
	if(cart_info){
		print_cart_info(cart);
		destroy_cart(cart);
		return 0;
	}

	// This is (or rather presently, will be) a bunch of checks to stop us from running ROMs we don't support yet. Eventually these will all be removed,
	// but for now we need to enforce this.
	if(!force_flag){
		if(cart->sys_type != NESFAMI){
			printf("AGNT-NES-Emulator only supports ROMs for the NES/Famicom.\n");
		}
	} else {
		printf("Warning: force flag specified, not running compatibility checks. Here be dragons!\n");
	}

	MMC mmc = new_MMC(cart, argv[argc-1]);
	

	// The NES doesn't actually have a proper MMU - this is here to work out which function to
	// send to the CPU so that opcode functions can't tell the difference between reading from the cartridge
	// and reading from RAM.
	MMU mmu = new_mmu(&mmc);

	// Before we start executing, we need to retrieve our reset vector, stored at 0xFFFC,
	// and stick it in the program counter. This tells us where to begin running code from.
	
	uint16_t start = cpu_read16(0xFFFC, &mmc);
	printf("Reset vector (0xFFFC): 0x%04X\n", start);

	CPU *cpu = new_cpu(&mmu);
	cpu->PC = start;

	// Enter fetch-decode-execute cycle.
	// For reference, the NES' PPU is clocked at 3 times the speed of the CPU,
	// so for every CPU clock we'll need to clock the PPU 3 times.
	while(!should_stop){
		// Tick CPU.	
		tick_cpu(cpu);
	
	}

	destroy_mmu(&mmu);
	destroy_mmc(&mmc);
	destroy_cart(cart);
	return 0;
}
