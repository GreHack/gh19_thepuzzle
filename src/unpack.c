#include "unpack.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dbg.h"

#define DEBUG 0

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
#if DEBUG
			fprintf(stderr, "[kidx=%u] %x %x %x %x\n", kidx, output[0] & 0xFF, output[1] & 0xFF, output[2] & 0xFF, output[3] & 0xFF); 
#endif
		}
		free(output);
	}
	return NULL;
}

int unpack(long pid, int offset)
{
	char *mem = dbg_read_mem(offset, 0x1000);
	int i = 0;
	int state = 0;
	/* Find correct key */
	char *rc4_key = rc4_get_key(mem);
	if (!rc4_key) {
#if DEBUG
		fprintf(stderr, "[err] key not found -- aborting\n");
#endif
		exit(1);
	}
	rc4_state_t *rstate = rc4_init(rc4_key, KLEN);
	while (1) {
		unsigned char x = rc4_stream(rstate);
#if DEBUG
		fprintf(stderr, "%02x ^ %02x -> ", (mem[i] & 0xFF), (x & 0xFF));
#endif
		mem[i] ^= x;
#if DEBUG
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
	free(rstate);
	return 0;
}
