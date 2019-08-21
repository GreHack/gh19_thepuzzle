#ifndef __UNPACK_H__
#define __UNPACK_H__

/* defines NKEY and KLEN */
#include "gen/rc4_consts.txt"
#include <stdint.h>

static char rc4_keys[NKEY][KLEN] = {
	/* import keys from file */
	#include "gen/rc4_keys.txt"
};

int unpack(uint64_t offset);
void reverse_jump(uint64_t addr);

#endif
