
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "dbg.h"

#define DEBUG 0

int unpack(long pid, int offset) {
	char *mem = dbg_read_mem(offset, 0x1000);
	int i = 0;
	int state = 0;
	while (1) {
		mem[i] ^= 0x42;
		if (((mem[i] & 0xFF) == 0x90) && (i > 2) && ((mem[i - 1] & 0xFF) == 0x90) && ((mem[i - 2] & 0xFF) != 0x90)) {
			state = state + 1;
		} else if (state == 2 && (mem[i] & 0xFF) == 0xc3) {
			break;
		}
		i += 1;
		if (i >= 0x1000)
			exit(1);
	}
	// FIXME for now, hard coding the 0x55 at the bg of the function
	/* because first byte was overwriten by 0xCC */
	mem[0] = 0x55;
	dbg_write_mem(offset, i+1, mem);
	return 0;
}
