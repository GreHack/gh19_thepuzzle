#include "dbg.h"
#include "core.h"
#include <stdio.h>
#include <sys/user.h>
#include <string.h>
#include <fcntl.h>

/* Right now, our debugger can debug a unique target.
 * It's cool because we don't have to pass the pid each time. */
static int g_pid;

/* We save here the base address of our program (not known because of PIE) */
static uint64_t g_baddr;

#define dbg_die(x) { \
	printf("==== FATAL: DBG DEAD -- %s - %s:%d\n", x, __FILE__, __LINE__); \
	perror("Reason:"); \
	kill(g_pid, SIGKILL); \
	exit(-1); \
	}

/*
 * Fetch our process base address.
 */
static void dbg_fetch_base_addr()
{
	int fd;
	char buf[16 + 1];

	// Warning: Maybe it won't work on some hardened linux kernels
 	// Note: This works (/proc/self/maps) when called from the father,
	// because using fork without calling execve won't change the program
	// base address (PIE/ASLR).
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
	g_baddr = strtoull(buf, NULL, 16);
}

/*
 * Attach to a process. Assumes the binary is PIE..
 */
void dbg_attach(int pid)
{
	// Note: PTRACE_ATTACH will send a SIGSTOP to the child
	g_pid = pid;
	long ret = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
	if (ret == -1) {
		dbg_die("Cannot attach");
	}

	// Fetch the base address, we will need it later
	dbg_fetch_base_addr();
}

/*
 * Add a breakpoint to the specified address.
 * We are in the context of PIE, so the given relative address will be rebased
 * on top of the program base address.
 */
long dbg_break(void *addr)
{
	// Because of PIE, we add the base address to the target addr
	addr = g_baddr + addr;

	// Get the original instruction
	long trap = ptrace(PTRACE_PEEKTEXT, g_pid, addr, NULL);
	if (trap == -1) {
		dbg_die("Cannot peek");
	}

	// Add a int3 instruction (0xcc *is* the INT3 instruction)
	trap = (trap & 0xffffffffffffff00) | 0xcc;

	// Write the instruction back
	// TODO We will probably want to save the old instruction
	// if we want to remove the breakpoint in the future
	// or maybe I didn't understand how ptrace_poketext works yet
	return ptrace(PTRACE_POKETEXT, g_pid, addr, trap);
}

/*
 * Tell the child process to continue.
 * Doesn't send any specific signal.
 */
void dbg_continue()
{
	// TODO: If we were stopped on a breakpoint, we might
	// want to restore the original instruction before
	// continuing ;)
	long ret = ptrace(PTRACE_CONT, g_pid, NULL, NULL);
	if (ret == -1) {
		dbg_die("Cannot continue the process");
	}
}

char *dbg_read_mem(int offset, int nb_bytes)
{
        char *buffer = (char *) malloc(nb_bytes);
	long ret;
	void *addr = (void *) g_baddr + offset;
	for (int i = 0; i < nb_bytes; i += 2) {
		ret = ptrace(PTRACE_PEEKTEXT, g_pid, addr + i, 0);	
		buffer[i] = ret & 0xFF;
		buffer[i + 1] = ret >> 8;
	}
	return buffer;
}

char *dbg_write_mem(int offset, int nb_bytes, char *data)
{
	void *addr = (void *) g_baddr + offset;
        int dword;
	for (int i = 0; i < nb_bytes/4; i++) {
		dword = ((int *) data)[i];
		ptrace(PTRACE_POKETEXT, g_pid, addr + 4*i, dword);
	}
}

void dbg_show_mem(int offset, int len)
{
	char *mem = dbg_read_mem(offset, len);
	for (int i = 0; i < len; i++) {
		printf("%02X ", (mem[i] & 0xFF));
		if (i % 16 == 7)
			printf("  ");
		else if (i % 16 == 15)
			printf("\n");
	}
	printf("\n");
	free(mem);
}

struct user_regs_struct *dbg_get_regs(void)
{
	struct user_regs_struct *regs = (struct user_regs_struct *) malloc(sizeof(struct user_regs_struct));
	ptrace(PTRACE_GETREGS, g_pid, NULL, regs);
	return regs;
}

void dbg_set_regs(struct user_regs_struct *regs)
{
	ptrace(PTRACE_SETREGS, g_pid, NULL, regs);
}
