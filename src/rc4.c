#include "rc4.h"

#include <stdlib.h>

struct rc4_state_s {
	unsigned char *s;
	int i;
	int j;
};

rc4_state_t *rc4_init(const char *key, int key_len)
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

char *rc4_crypt(const char *key, int key_len, char *input, int input_len) 
{
	char *output = (char *) malloc(input_len);
	rc4_state_t *state = rc4_init(key, key_len);
	for (int idx = 0; idx < input_len; idx++) {
		output[idx] = input[idx] ^ rc4_stream(state);
	}
	free(state);
	return output;
}
