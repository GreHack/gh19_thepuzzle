#include "unpack.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dbg.h"
#include "global.h"
#include "rc4.h"

const char *unpack_get_key(uint8_t *mem)
{
	unsigned int kidx = 0;
	for (kidx = 0; kidx < NKEY; kidx++) {
		char *output = rc4_crypt(rc4_keys[kidx], KLEN, (char*) mem, 4);
		if (strncmp(output + 1, "\x48\x89\xe5", 3) == 0) {
#ifdef DEBUG_2PAC
			fprintf(stderr, "key found: idx %u\n", kidx);
#endif
			return rc4_keys[kidx];
		} else {
#ifdef DEBUG_2PAC
			fprintf(stderr, "[kidx=%03u] K: %02x%02x%02x -> %02x %02x %02x\n", kidx, rc4_keys[kidx][0] & 0xFF, rc4_keys[kidx][1] & 0xFF, rc4_keys[kidx][2] & 0xFF, output[1] & 0xFF, output[2] & 0xFF, output[3] & 0xFF); 
#endif
		}
		FREE(output);
	}
	return NULL;
}

#define MAX_UNPACK 3
#define NFUNC 64
static uint64_t already_packed[NFUNC] = { 0 };
static uint64_t already_packed_end[NFUNC] = { 0 };
static uint64_t map_du_pauvre[NFUNC] = { 0 };
static int map_du_pauvre_val[NFUNC] = { 0 };

int unpack(uint64_t offset)
{
	// Default encrypt = -1 means we will decrypt
	int encrypt = -1;
	int j = 0;
	int state = 0;
	const char *rc4_key;
	const int FUNC_MAX_SIZE = 0x1000;

	// Check if we want to encrypt or decrypt the function
	for (int i = 0; i < NFUNC; i++) {
		if (already_packed_end[i] == offset) {
			encrypt = i;
			offset = already_packed[i];
			break;
		}
	}
#ifdef DEBUG_2PAC
	fprintf(stderr, "\n-----------> 2PAC (De/En)crypt: %d (0x%lx)\n", encrypt, offset);
#endif

	// For performance purpose, we check how many time this function was packed
	// and repacked in the past
	if (encrypt == -1) {
		for (j = 0; j < NFUNC; j++) {
			if (map_du_pauvre[j] == offset || map_du_pauvre[j] == 0) {
				break;
			}
		}

		if (map_du_pauvre_val[j] >= MAX_UNPACK) {
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, "Skipping unpacking... Breakpoints should have been deleted, this message should *never* appear\n");
#endif
			goto end_unpack;
		} else {
			map_du_pauvre[j] = offset;
			map_du_pauvre_val[j]++;
		}
	} else {
		for (j = 0; j < NFUNC; j++) {
			if (map_du_pauvre[j] == offset) {
				break;
			}
		}
		if (j != NFUNC && map_du_pauvre_val[j] >= MAX_UNPACK) {
			// fprintf(stderr, "Skipping repacking and disabling breakpoints...\n");
			dbg_breakpoint_delete_off(offset, true);
			dbg_breakpoint_delete_off(already_packed_end[encrypt], true);
			// Before continuing, since we deleted the breakpoint, continue won't
			// do the 1 byte substraction to RIP, so let's just do it here manually
			struct user_regs_struct *regs = dbg_regs_get();
			regs->rip = regs->rip - 1;
			dbg_regs_set(regs);
			free(regs);
			goto end_unpack;
		}
	}

	// Disable breakpoints to work on proper memory
	dbg_breakpoint_disable(offset, FUNC_MAX_SIZE);
	// Read the memory from the beginning of the function
	uint8_t *mem = dbg_mem_read(offset, FUNC_MAX_SIZE);

	// Get the rc4 key for encryption or decryption
	if (encrypt == -1) {
		rc4_key = unpack_get_key(mem);
		if (!rc4_key) {
#ifdef DEBUG_2PAC
			fprintf(stderr, "[err] key not found for function at 0x%lx -- aborting\n", offset);
#endif
			exit(1);
		}
	} else {
		rc4_key = rc4_keys[rand() % 256];
	}

	// Start the en/decryption
	rc4_state_t *rstate = rc4_init(rc4_key, KLEN);
	int i = 0;
	while (1) {
		unsigned char x = rc4_stream(rstate);
#ifdef DEBUG_2PAC
		fprintf(stderr, "%06lx: %02x ^ %02x -> ", offset + i, (mem[i] & 0xFF), (x & 0xFF));
#endif
		mem[i] ^= x;
#ifdef DEBUG_2PAC
		fprintf(stderr, "%02x\n", (mem[i] & 0xFF));
#endif
		if (encrypt == -1) {
			if (((mem[i] & 0xFF) == 0x90) && (i > 2) && ((mem[i - 1] & 0xFF) == 0x90) && ((mem[i - 2] & 0xFF) != 0x90)) {
				state = state + 1;
			} else if (state == 2 && (mem[i] & 0xFF) == 0xc3) {
				break;
			}
		} else {
			if (offset + i == already_packed_end[encrypt]) {
				break;
			}
		}
		i += 1;
	}

	// Update the encryption/decryption map to know the beginning of the function
	if (encrypt == -1) {
		mem[0] = 0x55;
		for (int j = 0; j < NFUNC; j++) {
			if (already_packed[j] == 0) {
				already_packed[j] = offset;
				already_packed_end[j] = offset + i;
				break;
			}
		}
	} else {
		already_packed[encrypt] = 0;
		already_packed_end[encrypt] = 0;
	}

	// Write the new program to memory and reenable breakpoints
	dbg_mem_write(offset, i+1, mem);
	dbg_breakpoint_enable(offset, FUNC_MAX_SIZE, true);

	// Free the memory
	rc4_free(rstate);
	FREE(mem);
	
	// Force ret execution when repacking (because 'ret' (0xc3) is now packed)
	if (encrypt != -1) dbg_action_ret();

#if 0
	if (encrypt == -1) {
		fprintf(stderr, "Unpacking 0x%lx\n", offset);
	} else {
		fprintf(stderr, "Repacking 0x%lx\n", offset);
	}
#endif
end_unpack:
	return 0;
}

