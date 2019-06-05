#include "dbg.h"
#include "core.h"
#include <stdio.h>
#include <sys/user.h>

/* Right now, our debugger can debug a unique target.
 * It's cool because we don't have to pass the pid each time. */
static int mpid;

#define dbg_die() { \
	printf("==== FATAL: DBG DEAD -- %s:%d\n", __FILE__, __LINE__); \
	kill(mpid, SIGKILL); \
	exit(-1); \
	}

void dbg_attach(int pid)
{
	// Note: PTRACE_ATTACH will send a SIGSTOP to the child
	// Note (2): After some experiments, it seems it doesn't
	mpid = pid;
	long ret = ptrace(PTRACE_ATTACH, pid, NULL, NULL);
	if (ret == -1) {
		dbg_die();
	}
}

long dbg_break(void *addr)
{
	// TODO
	//// Temporary hack to approximate PIE addr
	//{
	//	struct user_regs_struct regs;
	//	ptrace(PTRACE_GETREGS, mpid, NULL, &regs);
	//	void *naddr = (regs.rip & 0xffffffffffff0000) | (unsigned long long) addr;
	//	printf("Cheat: (old, RIP, new) -> (%p, %p, %p)\n", addr, regs.rip, naddr);
	//	addr = naddr;
	//}

	printf("Trying to break at: %p\n", addr);
	// Get the original instruction
	long trap = ptrace(PTRACE_PEEKTEXT, mpid, addr, NULL);
	if (trap == -1) {
		dbg_die();
	}

	// Add a int3 instruction
	trap = (trap & 0xffffffffffffff00) | 0xcc;

	// Write the instruction back
	return ptrace(PTRACE_POKETEXT, mpid, addr, trap);
}
