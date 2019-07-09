#include "dbg.h"
#include "core.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

#include "unpack.h"

/* Right now, our debugger can debug a unique target.
 * It's cool because we don't have to pass the pid each time. */
static int g_pid;

/* We save here the base address of our program (not known because of PIE) */
static uint64_t g_baddr;

/* Breakpoint structure, used to save the original data */
typedef struct debug_breakpoint_t {
	uint64_t addr;
	uint8_t orig_data;
	uint64_t handler;
} dbg_bp;

typedef struct debug_breakpoint_node_t {
	dbg_bp *data;
	struct debug_breakpoint_node_t *next;
} dbg_bp_node;

/* Our breakpoints list head */
dbg_bp_node *breakpoints = NULL;

static void bp_node_append(dbg_bp *data) {
	if (!breakpoints) {
		dbg_die("List is not allocated");
	}
	dbg_bp_node *end = breakpoints;
	while (end->next) {
		end = end->next;
	}
	dbg_bp_node *newnode = malloc(sizeof(dbg_bp_node));
	newnode->data = NULL;
	newnode->next = NULL;
	end->data = data;
	end->next = newnode;
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

	// Alloc our list head
	if (!breakpoints) {
		breakpoints = malloc(sizeof(dbg_bp_node));
		if (!breakpoints) {
			dbg_die("Cannot reserve space for breakpoints");
		}
		breakpoints->data = NULL;
		breakpoints->next = NULL;
	}

	// Fetch the base address, we will need it later
	dbg_fetch_base_addr();
}

/*
 * Add a breakpoint to the specified address.
 * We are in the context of PIE, so the given relative address will be rebased
 * on top of the program base address.
 */
void dbg_break(void *addr)
{
	dbg_break_handler(addr, NULL);
}

void dbg_break_handler(void *addr, void *handler)
{
	// Because of PIE, we add the base address to the target addr
	addr = g_baddr + addr;

	// Get the original instruction
	long trap = ptrace(PTRACE_PEEKTEXT, g_pid, addr, NULL);
	if (trap == -1) {
		dbg_die("Cannot peek");
	}
	fprintf(stderr, "Instruction at %p: %lx\n", addr, trap);

	dbg_bp *bp = malloc(sizeof(dbg_bp));
	bp->addr = (uint64_t) addr;
	bp->orig_data = trap & 0xff;
	bp->handler = handler;
	bp_node_append(bp);

	// Add a int3 instruction (0xcc *is* the INT3 instruction)
	trap = (trap & 0xffffffffffffff00) | 0xcc;

	// Write the instruction back
	if (ptrace(PTRACE_POKETEXT, g_pid, addr, trap) == -1) {
		dbg_die("Could not insert breakpoint");
	}
}

/*
 * Tell the child process to continue.
 * Doesn't send any specific signal.
 */
void dbg_continue(bool restore)
{
	struct user_regs_struct regs;
	ptrace(PTRACE_GETREGS, g_pid, NULL, &regs);
	fprintf(stderr, "Resuming at addr: 0x%llx\n", regs.rip);

	dbg_bp_node *ptr = breakpoints;
	dbg_bp *bp = NULL;
	while (ptr && ptr->next) {
		bp = ptr->data;
		fprintf(stderr, "VAL, VAL: %llx, %lx\n", regs.rip, bp->addr);
		if (bp->addr == regs.rip - 1) {
			break;
		}
		bp = NULL;
		ptr = ptr->next;
	}

	// We reached one of our breakpoints
	if (bp) {
		// We executed an int3 execution
		// We have to roll back one byte before
		regs.rip -= 1;
		if (ptrace(PTRACE_SETREGS, g_pid, NULL, &regs) == -1) {
			dbg_die("Cannot set new registers value");
		}

		if (regs.rip != bp->addr) {
			dbg_die("WHAT THE FUCK IS GOING ON?!");
		}

		// Replace the breakpoint by its original instruction
		long orig = ptrace(PTRACE_PEEKTEXT, g_pid, bp->addr, NULL);
		if (orig == -1) {
			dbg_die("Cannot get the original instruction");
		}
		fprintf(stderr, "Current instruction: %lx\n", orig);
		if (restore) {
			fprintf(stderr, "Breakpoint original instruction: %x\n", bp->orig_data);
			long newdata = (orig & 0xffffffffffffff00) | bp->orig_data;
			fprintf(stderr, "OK, actually restoring original instr\n");
			fprintf(stderr, "New instruction: %lx\n", newdata);
			if (ptrace(PTRACE_POKETEXT, g_pid, bp->addr, newdata) == -1) {
				dbg_die("Cannot modify the program instruction");
			}
		}
		// Single step
		if (ptrace(PTRACE_SINGLESTEP, g_pid, NULL, NULL) == -1) {
			dbg_die("Cannot singlestep");
		}
		// TODO Replace this sleep with a waitpid() or something
		sleep(0.1);

		// Rewrite the breakpoint and continue
		if (ptrace(PTRACE_POKETEXT, g_pid, bp->addr, orig) == -1) {
			dbg_die("Cannot modify the program instruction");
		}

		if (ptrace(PTRACE_CONT, g_pid, NULL, NULL) == -1) {
			dbg_die("Cannot continue the process");
		}
	} else {
		fprintf(stderr, "WARNING: Continue reason not handled\n");
		if (ptrace(PTRACE_CONT, g_pid, NULL, NULL) == -1) {
			dbg_die("Cannot continue the process");
		}
	}
}

/*
 * Read memory from the debuggee.
 * The returned buffer must be freed.
 */
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

/*
 * Write debuggee's memory.
 */
void dbg_write_mem(int offset, int nb_bytes, char *data)
{
	void *addr = (void *) g_baddr + offset;
	int dword;
	for (int i = 0; i <= nb_bytes/4; i++) {
		dword = ((int *) data)[i];
		ptrace(PTRACE_POKETEXT, g_pid, addr + 4*i, dword);
	}
}

void dbg_show_mem(int offset, int len)
{
	char *mem = dbg_read_mem(offset, len);
	long addr = g_baddr + offset;
	printf("[%016lx] ", addr);
	for (int i = 0; i < len; i++) {
		printf("%02X ", (mem[i] & 0xFF));
		if (i % 16 == 7)
			printf("  ");
		else if (i % 16 == 15)
			printf("\n[%016lx] ", addr + i);
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

void dbg_break_handle(uint64_t rip)
{
	dbg_bp_node *ptr = breakpoints;
	dbg_bp *bp = NULL;
	while (ptr && ptr->next) {
		bp = ptr->data;
		if (bp->addr == rip - 1) {
			break;
		}
		bp = NULL;
		ptr = ptr->next;
	}
	if (!bp) {
		printf("WARNING: Expected a breakpoint but there was none heh");
		return;
	}

	if (bp->handler) {
		printf(">>>>>> CALLING HANDLER 0x%lx!\n", bp->handler);
		int (*handler_func)(long, int) = g_baddr + bp->handler;
		handler_func(g_pid, rip - g_baddr - 1);
	}

	printf(">>>>>> Warning: Automatic continue after handling breakpoint!\n");
	dbg_continue((bp->handler + g_baddr) != unpack);
}
