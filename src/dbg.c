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
	uint64_t addr; // Must be an actual memory address, not an offset
	uint8_t orig_data;
	uint64_t handler; // TODO Maybe use a memory address as well to be consistent with addr
	const char *uhandler; // User defined function for handling a bp
} dbg_bp;

typedef struct debug_breakpoint_node_t {
	dbg_bp *data;
	struct debug_breakpoint_node_t *next;
} dbg_bp_node;

/* Our breakpoints list head */
dbg_bp_node *breakpoints = NULL;

/* Structures for user specified debugger functions */
typedef struct dbg_function_line_t {
	char line[256];
	struct dbg_function_line_t *next;
} dbg_function_line;
typedef struct dbg_function_t {
	char name[64];
	dbg_function_line *firstline;
	struct dbg_function_t *next;
} dbg_function;

/* Our user defined functions list head */
dbg_function *functions = NULL;

static void bp_node_append(dbg_bp *data) {
	if (!breakpoints) {
		breakpoints = malloc(sizeof(dbg_bp_node));
		if (!breakpoints) {
			dbg_die("Cannot reserve space for breakpoints");
		}
		breakpoints->data = NULL;
		breakpoints->next = NULL;
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

static bool bp_node_delete(dbg_bp *data) {
	dbg_bp_node tmp = { NULL, breakpoints };
	dbg_bp_node *cur = &tmp;
	while (cur->next) {
		if (cur->next->data == data) {
			// Found the one, remove it
			dbg_bp_node *todelete = cur->next;
			cur->next = cur->next->next;
			if (todelete == breakpoints) {
				breakpoints = cur->next;
			}
			free(todelete);
			return true;
		}
		cur = cur->next;
	}
	return false;
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
void dbg_break(void *addr)
{
	dbg_break_handler(addr, NULL, NULL);
}

/*
 * Add a breakpoint with its handler function.
 * Note: Both are offsets because of PIE, not actual VA
 */
void dbg_break_handler(void *addr, void *handler, const char *uhandler)
{
	// Because of PIE, we add the base address to the target addr
	addr = g_baddr + addr;

	// Get the original instruction
	long trap = ptrace(PTRACE_PEEKTEXT, g_pid, addr, NULL);
	if (trap == -1) {
		printf("K %p\n", addr);
		dbg_die("Cannot peek");
	}
	fprintf(stderr, "Instruction at %p: %lx\n", addr, trap);

	dbg_bp *bp = malloc(sizeof(dbg_bp));
	bp->addr = (uint64_t) addr;
	bp->orig_data = trap & 0xff;
	bp->handler = (uint64_t) handler;
	if (uhandler) {
		// Copy handler name and remove newline
		bp->uhandler = strdup(uhandler); // TODO Free me
		char *tmp = bp->uhandler;
		while (*tmp) {
			if (*tmp == '\n') {
				*tmp = '\0';
				break;
			}
			tmp++;
		}
	} else {
		bp->uhandler = NULL;
	}
	bp_node_append(bp);

	// Add a int3 instruction (0xcc *is* the INT3 instruction)
	trap = (trap & 0xffffffffffffff00) | 0xcc;

	// Write the instruction back
	if (ptrace(PTRACE_POKETEXT, g_pid, addr, trap) == -1) {
		dbg_die("Could not insert breakpoint");
	}
}

/*
 * Delete a breakpoint
 */
void dbg_break_delete(void *addr)
{
	dbg_bp_node *ptr = breakpoints;
	dbg_bp *bp = NULL;
	while (ptr && ptr->next) {
		bp = ptr->data;
		if (bp->addr == (uint64_t) addr) {
			if (bp_node_delete(bp)) {
				free(bp);
			} else {
				fprintf(stderr, "ERROR: Could not delete breakpoint at %p\n", addr);
			}
		}
		bp = NULL;
		ptr = ptr->next;
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
	fprintf(stderr, "Resuming at addr: 0x%llx (%06x)\n", regs.rip, regs.rip - g_baddr);

	dbg_bp_node *ptr = breakpoints;
	dbg_bp *bp = NULL;
	while (ptr && ptr->next) {
		bp = ptr->data;
		// fprintf(stderr, "Looking for bp: RIP, BPADDR: %llx, %lx\n", regs.rip, bp->addr);
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
			//fprintf(stderr, "Breakpoint original instruction: %x\n", bp->orig_data);
			long newdata = (orig & 0xffffffffffffff00) | bp->orig_data;
			//fprintf(stderr, "OK, actually restoring original instr\n");
			fprintf(stderr, "New instruction: %lx\n", newdata);
			if (ptrace(PTRACE_POKETEXT, g_pid, bp->addr, newdata) == -1) {
				dbg_die("Cannot modify the program instruction");
			}
		}
		// Single step
		if (ptrace(PTRACE_SINGLESTEP, g_pid, NULL, NULL) == -1) {
			dbg_die("Cannot singlestep");
		}

		int status;
		waitpid(g_pid, &status, 0);
		if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP) {
			fprintf(stderr, "Unexpected signal received: %s\n", strsignal(WSTOPSIG(status)));
			dbg_die("WHAT THE FUCK WHY JOHN WHYYYYYYYY");
		}

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
uint8_t *dbg_read_mem(int offset, int nb_bytes)
{
	// Make sure the requested amount of bytes is aligned
	while (nb_bytes % sizeof(long)) {
		nb_bytes++;
	}
	uint8_t *buffer = malloc(nb_bytes);
	unsigned long ret;
	void *addr = (void *) g_baddr + offset;
	for (int i = 0; i < nb_bytes; i += sizeof(long)) {
		ret = ptrace(PTRACE_PEEKTEXT, g_pid, addr + i, 0);	
		buffer[i] = ret & 0xFF;
		buffer[i + 1] = (ret >>  8) & 0xFF;
		buffer[i + 2] = (ret >> 16) & 0xFF;
		buffer[i + 3] = (ret >> 24) & 0xFF;
		buffer[i + 4] = (ret >> 32) & 0xFF;
		buffer[i + 5] = (ret >> 40) & 0xFF;
		buffer[i + 6] = (ret >> 48) & 0xFF;
		buffer[i + 7] = (ret >> 56) & 0xFF;
	}
	return buffer;
}

/*
 * Write debuggee's memory.
 */
void dbg_write_mem(int offset, int nb_bytes, char *data)
{
	void *addr = (void *) g_baddr + offset;
	unsigned long word;
	for (int i = 0; i < nb_bytes; i += sizeof(long)) {
		int diff = nb_bytes - i;
		if (diff < sizeof(long)) {
			uint8_t *old_data = dbg_read_mem(offset + i, sizeof(long));
			word = ((long *)old_data)[0];
			word >>= diff * sizeof(long);
			word <<= diff * sizeof(long);
			unsigned long mask = *((unsigned long *) (data + i));
			mask <<= (sizeof(long) - diff) * sizeof(long);
			mask >>= (sizeof(long) - diff) * sizeof(long);
			word |= mask;
			free(old_data);
		} else {
			word = *((unsigned long *) (data + i));
		}
		//uint8_t *fu = dbg_read_mem(offset + i, nb_bytes);
		//fprintf(stderr, "---------BEFORE WRITE: (%x) %016lx\n", offset + i, *((uint64_t*)fu));
		//fprintf(stderr, "---------       WRITE:        %016lx\n", word);
		ptrace(PTRACE_POKETEXT, g_pid, addr + i, word);
		//fu = dbg_read_mem(offset + i, nb_bytes);
		//fprintf(stderr, "----------AFTER WRITE: (%x) %016lx\n", offset + i, *((uint64_t*)fu));
	}
}

void dbg_write_mem_va(uint64_t va, int nb_bytes, char *data)
{
	dbg_write_mem((int) (va - g_baddr), nb_bytes, data);
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

void dbg_breakpoint_disable(uint64_t offset, uint64_t size)
{
	dbg_bp_node *ptr = breakpoints;
	dbg_bp *bp = NULL;
	uint64_t va = g_baddr + offset;
	while (ptr && ptr->next) {
		bp = ptr->data;
		if (bp->addr >= va && bp->addr < (va + size)) {
			// Disable the breakpoint
			//fprintf(stderr, ":::::::::::::DISABLING BP\n");
			dbg_write_mem_va(bp->addr, 1, &bp->orig_data);
		}
		bp = NULL;
		ptr = ptr->next;
	}
}

void dbg_breakpoint_enable(uint64_t offset, uint64_t size)
{
	dbg_bp_node *ptr = breakpoints;
	dbg_bp *bp = NULL;
	uint64_t va = g_baddr + offset;
	while (ptr && ptr->next) {
		bp = ptr->data;
		if (bp->addr >= va && bp->addr < (va + size)) {
			// Enable the breakpoint
			//fprintf(stderr, ":::::::::::::ENABLING BP\n");
			dbg_write_mem_va(bp->addr, 1, "\xcc");
		}
		bp = NULL;
		ptr = ptr->next;
	}
}

/*
 * Replace a breakpoint original data
 */
void dbg_breakpoint_set_original_data(uint64_t offset, uint8_t data)
{

	dbg_bp_node *ptr = breakpoints;
	dbg_bp *bp = NULL;
	while (ptr && ptr->next) {
		bp = ptr->data;
		if (bp->addr == offset + g_baddr) {
			bp->orig_data = data;
			break;
		}
		bp = NULL;
		ptr = ptr->next;
	}
}

bool dbg_register_function(const char* firstline, FILE *fileptr)
{
	dbg_function *func = calloc(1, sizeof(dbg_function));
	const char func_end[] = "end";

	// Set function name
	char *funcnameptr = strrchr(firstline, ' ') + 1;
	strncpy(func->name, funcnameptr, sizeof(func->name));
	/// Replace \n with \0
	funcnameptr = func->name;
	while (*funcnameptr) {
		if (*funcnameptr == '\n') {
			*funcnameptr = '\0';
		}
		funcnameptr++;
	}
	/// Make sure it ends with a nullbyte
	func->name[sizeof(func->name) - 1] = '\0';

	// Parse the rest of the file
	char *line = NULL;
	size_t nb;
	bool res = false;

	dbg_function_line **prevline = &func->firstline;
	while (getline(&line, &nb, fileptr) != -1) {
		// We reached the end of the function, parsing was successful
		if (!strncmp(func_end, line, sizeof(func_end) - 1)) {
			res = true;
			break;
		}

		dbg_function_line *fline = calloc(1, sizeof(dbg_function_line));
		strncpy(fline->line, line, sizeof(dbg_function_line));
		*prevline = fline;
		prevline = &fline->next;

		if (line) {
			free(line);
			line = NULL;
			nb = 0;
		}
	}

	// Add the function to functions list
	if (res) {
		if (!functions) {
			functions = func;
		} else {
			dbg_function *f = functions;
			while (f->next) {
				f = f->next;
			}
			f->next = func;
		}
	}

	return res;
}

static void dbg_function_call(const char *uhandler)
{
	dbg_function *fptr = functions;
	while (fptr) {
		// Execute the function
		if (!strcmp(uhandler, fptr->name)) {
			fprintf(stderr, "Executing Func: %s\n", fptr->name);
			dbg_function_line *line = fptr->firstline;
			while (line) {
				fprintf(stderr, "  -> %s", line->line);
				dbg_parse_command(line->line);
				line = line->next;
			}
			break;
		}

		// Switch to next function
		fptr = fptr->next;
	}
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
		int (*handler_func)(uint64_t) = g_baddr + bp->handler;
		handler_func(rip - g_baddr - 1);
	}
	if (bp->uhandler) {
		printf(">>>>>> CALLING USER HANDLER '%s'!\n", bp->uhandler);
		dbg_function_call(bp->uhandler);
	}

	printf(">>>>>> Warning: Automatic continue after handling breakpoint!\n");
	dbg_continue((bp->handler + g_baddr) != unpack);
}

