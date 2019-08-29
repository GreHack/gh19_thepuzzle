#include "unpack.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dbg.h"

typedef struct {
	unsigned char *s;
	int i;
	int j;
} rc4_state_t;

rc4_state_t *rc4_init(char *key, int key_len)
{
	rc4_state_t *state = (rc4_state_t *) malloc(sizeof(rc4_state_t));
	state->s = (unsigned char *) malloc(256);
	unsigned char i = 0;
	unsigned char j = 0;
	do {
		state->s[i] = i;
		i++;
	} while (i != 0);
	do {
		j = (j ^ state->s[i] ^ key[i % key_len]) % 256;
		unsigned char tmp = state->s[i];
		state->s[i] = state->s[j];
		state->s[j] = tmp;
		i++;
	} while (i != 0);
	state->i = 0;
	state->j = 0;
	return state;
}

unsigned char rc4_stream(rc4_state_t *state)
{
	unsigned char tmp; 
	state->i = (state->i + 1) % 256;
	state->j = (state->j + state->s[state->i]) % 256;
	tmp = state->s[state->i];
	state->s[state->i] = state->s[state->j];
	state->s[state->j] = tmp;
	return state->s[(state->s[state->i] + state->s[state->j]) % 256];
}

char *rc4_crypt(char *key, int key_len, char *input, int input_len) 
{
	char *output = (char *) malloc(input_len);
	rc4_state_t *state = rc4_init(key, key_len);
	for (int idx = 0; idx < input_len; idx++) {
		output[idx] = input[idx] ^ rc4_stream(state);
	}
	free(state);
	return output;
}

char *rc4_get_key(char *mem)
{
	unsigned int kidx = 0;
	for (kidx = 0; kidx < NKEY; kidx++) {
		char *output = rc4_crypt(rc4_keys[kidx], KLEN, mem, 4);
		if (strncmp(output + 1, "\x48\x89\xe5", 3) == 0) {
			fprintf(stderr, "key found: idx %u\n", kidx);
			return rc4_keys[kidx];
		} else {
#ifdef DEBUG_2PAC
			fprintf(stderr, "[kidx=%u] %x %x %x %x\n", kidx, output[0] & 0xFF, output[1] & 0xFF, output[2] & 0xFF, output[3] & 0xFF); 
#endif
		}
		free(output);
	}
	return NULL;
}

int unpack(uint64_t offset)
{
	dbg_breakpoint_disable(offset + 1, 0x1000);
	uint8_t *mem = dbg_read_mem(offset, 0x1000);

	int i = 0;
	int state = 0;
	/* Find correct key */
	char *rc4_key = rc4_get_key(mem);
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
		fprintf(stderr, "%06x: %02x ^ %02x -> ", offset + i, (mem[i] & 0xFF), (x & 0xFF));
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
		if (i >= 0x100) {
			fprintf(stderr, "Something's wrong... aborting\n");
			exit(1);
		}
	}
	mem[0] = 0x55;
	dbg_write_mem(offset, i+1, mem);
	dbg_breakpoint_enable(offset + 1, 0x1000);
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

