
#include "check.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "sha256.h"
#include "gen/flag.h"

#include <pthread.h>
#include <time.h>
#include <unistd.h>

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
	FREE(ctx);
	bool res = 0 == strncmp((char *) input_hash, (char *) flag_hash, 32);
	TUPAC_END
	return res;
}

static void time_die()
{
#ifdef DEBUG_DEBUGGER
	fprintf(stderr, "Dead because time has passed!\n");
#endif
	fprintf(stdout, "Come on, give me some input to process, man.\n");
	exit(0);
}

static void *check_time(void *useless)
{
	if (useless == NULL) {
		exit(3);
	}
	clock_t first = clock();
	do {
		clock_t diff = clock() - first;
		int nbsec = diff / CLOCKS_PER_SEC;
		if (nbsec >= CHILD_MAX_SLEEP_TIME) {
			time_die();
		}
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, "TIME HAS PASSED: %d!\n", nbsec);
#endif
		sleep(1);
	} while (1);
	return NULL;
}

int start_timer(char *useless)
{
	pthread_t check_time_thread;
	if (pthread_create(&check_time_thread, NULL, check_time, useless)) {
		return 1;
	}
	return 0;
}
