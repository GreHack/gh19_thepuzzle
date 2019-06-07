#include "dbg.h"
#include "core.h"
#include <stdio.h>
#include <sys/user.h>
#include <string.h>
#include <fcntl.h>

/* Right now, our debugger can debug a unique target.
 * It's cool because we don't have to pass the pid each time. */
static int mpid;

/* We save here the base address of our program (not known because of PIE) */
static uint64_t baddr;

#define dbg_die(x) { \
	printf("==== FATAL: DBG DEAD -- %s - %s:%d\n", x, __FILE__, __LINE__); \
	kill(mpid, SIGKILL); \
	exit(-1); \
	}

static void dbg_fetch_base_addr()
{
	int fd;
	char buf[16 + 1];

	// Warning: Maybe it won't work on some hardened linux kernels
	fd = open("/proc/self/maps", O_RDONLY);
	if (fd == -1) {
		dbg_die("Failed to open /proc/self/maps, cannot get its own base address");
	}

	// We assume the first line contains our base address
	read(fd, buf, 16);
	buf[16] = '\0';
	close(fd);

	// Remove any read '-' and place a null byte
	char *dash = strchr(buf, '-');
	if (dash) {
		*dash = '\0';
	}

	// Convert it to what we want
	long long val = strtoull(buf, NULL, 16);

	baddr = val;
}

void dbg_attach(int pid)
{
	// Note: PTRACE_ATTACH will send a SIGSTOP to the child
	// Note (2): After some experiments, it seems it doesn't - TODO: fix the waitpid in the debugger
	mpid = pid;
	long ret = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
	if (ret == -1) {
		dbg_die("Cannot attach");
	}

	// Fetch the base address, we will need it later
	dbg_fetch_base_addr();
}

long dbg_break(void *addr)
{
	// Because of PIE, we add the base address to the target addr
	addr = baddr + addr;

	// Get the original instruction
	long trap = ptrace(PTRACE_PEEKTEXT, mpid, addr, NULL);
	if (trap == -1) {
		dbg_die("Cannot peek");
	}

	// Add a int3 instruction
	trap = (trap & 0xffffffffffffff00) | 0xcc;

	// Write the instruction back
	return ptrace(PTRACE_POKETEXT, mpid, addr, trap);
}

