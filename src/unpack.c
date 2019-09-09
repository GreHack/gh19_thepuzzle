#include "unpack.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dbg.h"
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
			fprintf(stderr, "[kidx=%03u] K: %02x%02x%02x -> %02x %02x %02x\n", kidx, rc4_keys[kidx][0] & 0xFF, rc4_keys[kidx][1] & 0xFF, rc4_keys[kidx][2] & 0xFF, output[0] & 0xFF, output[1] & 0xFF, output[2] & 0xFF); 
#endif
		}
		free(output);
	}
	return NULL;
}

int unpack(uint64_t offset)
{
	dbg_breakpoint_disable(offset + 1, 0x1000);
	uint8_t *mem = dbg_mem_read(offset, 0x1000);

	int i = 0;
	int state = 0;
	/* Find correct key */
	const char *rc4_key = unpack_get_key(mem);
	if (!rc4_key) {
#ifdef DEBUG_2PAC
		fprintf(stderr, "[err] key not found -- aborting\n");
#endif
		exit(1);
	}
	rc4_state_t *rstate = rc4_init(rc4_key, KLEN);
	while (1) {
		unsigned char x = rc4_stream(rstate);
#ifdef DEBUG_2PAC
		fprintf(stderr, "%06lx: %02x ^ %02x -> ", offset + i, (mem[i] & 0xFF), (x & 0xFF));
#endif
		mem[i] ^= x;
#ifdef DEBUG_2PAC
		fprintf(stderr, "%02x\n", (mem[i] & 0xFF));
#endif
		if (((mem[i] & 0xFF) == 0x90) && (i > 2) && ((mem[i - 1] & 0xFF) == 0x90) && ((mem[i - 2] & 0xFF) != 0x90)) {
			state = state + 1;
		} else if (state == 2 && (mem[i] & 0xFF) == 0xc3) {
			break;
		}
		i += 1;
		// I don't know why this was added in a first place?
		// But that's definitely wrong, some functions are bigger than 0x100
		/*if (i >= 0x100) {
			fprintf(stderr, "Something's wrong... aborting\n");
			exit(1);
		}*/
	}
	mem[0] = 0x55;
	dbg_mem_write(offset, i+1, mem);
	dbg_breakpoint_enable(offset + 1, 0x1000, true);
	free(rstate);
	free(mem);
	return 0;
}

void reverse_jump(uint64_t offset)
{
	// Okay so it's kinda ugly but anyway
	// We wrote a CC (or FF after unpacking) at this location,
	// so let's just tell the debugger it's a 0x7c
	fprintf(stderr, "[INFO] Reversing jump at %p!\n", (void *) offset);
	dbg_breakpoint_set_original_data(offset, 0x7c);
}

