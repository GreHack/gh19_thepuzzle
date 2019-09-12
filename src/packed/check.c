
#include "check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "sha256.h"
#include "flag.h"

bool check_flag(char *input)
{
	TUPAC_BEG
	unsigned char input_hash[32];
	SHA256_CTX *ctx = (SHA256_CTX *) malloc(sizeof(SHA256_CTX));;
	sha256_init(ctx);
	sha256_update(ctx, (unsigned char *) input, strlen(input));
	sha256_final(ctx, input_hash);
	for (unsigned int i = 0; i < 32; i++)
		if (i % 2)
			input_hash[i] ^= 0x19;
		else
			flag_hash[i] ^= 0x19;
#ifdef DEBUG_CHECK
	fprintf(stderr, "user input: %s\n", input);
	fprintf(stderr, "input hash: ");
	for (unsigned int i = 0; i < 32; i++)
		fprintf(stderr, "%02x", input_hash[i]);
	fprintf(stderr, "\n");
	fprintf(stderr, " flag hash: ");
	for (unsigned int i = 0; i < 32; i++)
		fprintf(stderr, "%02x", 0xff & flag_hash[i]);
	fprintf(stderr, "\n"); 
#endif
	bool res = 0 == strncmp((char *) input_hash, (char *) flag_hash, 32);
	TUPAC_END
	return res;
}
