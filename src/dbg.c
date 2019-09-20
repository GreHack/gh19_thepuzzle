#include "dbg.h"
#include "core.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>

#include "global.h"
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
	char *uhandler; // User defined function for handling a bp
	char *dhandler; // Death handler function that properly fixes the code once the bp is deleted
	uint64_t calls; // Represent the remaining allowed calls to that breakpoint
	struct debug_breakpoint_t *next;
} dbg_bp;

/* Our breakpoints list head */
dbg_bp *breakpoints = NULL;

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

asm("mem_jmp_array: .fill 600");

#ifdef DEBUG_DEBUGGER
void dbg_breakpoint_debugprint()
{
	dbg_bp *cur = breakpoints;
	while (cur->next) {
		fprintf(stderr, "{ 0x%lx, 0x%lx, 0x%02x, %p, %p }", cur->addr, cur->handler, cur->orig_data, cur->uhandler, cur->next);
		fprintf(stderr, " ---> ");
		cur = cur->next;
	}
	fprintf(stderr, "\n");
}
#endif

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
void dbg_breakpoint_add(void *addr)
{
	dbg_breakpoint_add_handler(addr, NULL, NULL, NULL);
}

/*
 * Add a breakpoint with its handler function.
 * Note: Both are offsets because of PIE, not actual VA
 */
void dbg_breakpoint_add_handler(void *addr, void *handler, const char *uhandler, const char *dhandler)
{
	// Because of PIE, we add the base address to the target addr
	addr = g_baddr + addr;

	// Get the original instruction
	long trap = ptrace(PTRACE_PEEKTEXT, g_pid, addr, NULL);
	if (trap == -1) {
		printf("K %p\n", addr);
		dbg_die("Cannot peek");
	}

	dbg_bp *bp = malloc(sizeof(dbg_bp));
	bp->addr = (uint64_t) addr;
	bp->orig_data = trap & 0xff;
	bp->handler = (uint64_t) handler;
	bp->next = NULL;
	bp->calls = -1; // Infinite calls to this function are allowed

	// Handle user handler
	if (uhandler && *uhandler) {
		// Copy handler name and remove newline
		bp->uhandler = strdup(uhandler);
		char *tmp = (char *) bp->uhandler;
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

	// Handle death handler
	if (dhandler && *dhandler) {
		bp->dhandler = strdup(dhandler);
		char *tmp = (char *) bp->dhandler;
		while (*tmp) {
			if (*tmp == '\n') {
				*tmp = '\0';
				break;
			}
			tmp++;
		}
	} else {
		bp->dhandler = NULL;
	}

	if (bp->uhandler || bp->dhandler) {
		bp->calls = MAX_BP_CALL;
	}

	// Now add the breakpoint to the list
	if (!breakpoints) {
		breakpoints = bp;
	} else {
		dbg_bp *head = breakpoints;
		while (head->next) {
			head = head->next;
		}
		head->next = bp;
	}

	// Add a int3 instruction (0xcc *is* the INT3 instruction)
	trap = (trap & 0xffffffffffffff00) | 0xcc;

	// Write the instruction back
	if (ptrace(PTRACE_POKETEXT, g_pid, addr, trap) == -1) {
		dbg_die("Could not insert breakpoint");
	}

#ifdef DEBUG_DEBUGGER
	fprintf(stderr, "Added BP at %p (0x%lx) (%06lx, %16s, %16s) (%p)\n", addr, (uint64_t) addr - g_baddr, (uint64_t) bp->handler, bp->uhandler, bp->dhandler, bp);
#endif
}

/*
 * Delete a breakpoint
 */
void dbg_breakpoint_delete(uint64_t addr, bool restore)
{
	// fprintf(stderr, "Deleting bp at %lx\n", addr - g_baddr);
	if (!breakpoints) {
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, "ERROR: No breakpoints, cannot delete\n");
#endif
		return;
	}
	dbg_bp tmp = { 0 };
	tmp.next = breakpoints;

	dbg_bp *head = &tmp;
	dbg_bp *old = NULL;
	while (head && head->addr != addr) {
		old = head;
		head = head->next;
	}
	if (head) {
		old->next = head->next;
		if (head == breakpoints) {
			breakpoints = head->next;
		}
		if (restore) {
#ifdef DEBUG_DEBUGGER
			uint8_t *mem = dbg_mem_read_va(head->addr, 1);
			fprintf(stderr, "Restored original value during bp deletion: %02x -> %02x\n", *mem, head->orig_data);
			FREE(mem);
#endif
			dbg_mem_write_va(head->addr, 1, (uint8_t*) &head->orig_data);
		}
		FREE(head->uhandler);
		FREE(head->dhandler);
		FREE(head);
	} else {
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, "WARNING: Could not find breakpoint: %lx\n", addr - g_baddr);
		dbg_die("Could not delete breakpoint!");
#endif
	}
}

void dbg_breakpoint_delete_off(uint64_t offset, bool restore)
{
	dbg_breakpoint_delete(g_baddr + offset, restore);
}

void dbg_breakpoint_disable(uint64_t offset, uint64_t size)
{
	dbg_bp *ptr = breakpoints;
	uint64_t va = g_baddr + offset;
	while (ptr) {
		if (ptr->addr >= va && ptr->addr < (va + size)) {
			// Disable the breakpoint
#ifdef DEBUG_DEBUGGER
			uint8_t *mem = dbg_mem_read_va(ptr->addr, 1);
#endif
			dbg_mem_write_va(ptr->addr, 1, (uint8_t*) &ptr->orig_data);
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, "/!\\ Disabled breakpoint at 0x%016lx (0x%lx) (%02x -> %02x)\n", ptr->addr, ptr->addr - g_baddr, *mem, ptr->orig_data);
			free(mem);
#endif
		}
		ptr = ptr->next;
	}
}

void dbg_breakpoint_enable(uint64_t offset, uint64_t size, bool restore_original)
{
	dbg_bp *ptr = breakpoints;
	uint64_t va = g_baddr + offset;
	while (ptr) {
		if (ptr->addr >= va && ptr->addr < (va + size)) {
			// Enable the breakpoint
			if (restore_original) {
				uint8_t *data = dbg_mem_read_va(ptr->addr, 1);
#ifdef DEBUG_DEBUGGER
				fprintf(stderr, "Overwrote breakpoint original value at 0x%lx (0x%02x -> 0x%02x)\n", ptr->addr - g_baddr, ptr->orig_data, *data);
#endif
				ptr->orig_data = *data;
				FREE(data);
			}
#ifdef DEBUG_DEBUGGER
			uint8_t *data = dbg_mem_read_va(ptr->addr, 1);
#endif
			dbg_mem_write_va(ptr->addr, 1, (uint8_t*)"\xcc");
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, "/!\\ Enabled breakpoint at 0x%016lx (0x%lx) (%02x -> %02x)\n", ptr->addr, ptr->addr - g_baddr, *data, 0xcc);
			free(data);
#endif
		}
		ptr = ptr->next;
	}
}

/*
 * Replace a breakpoint original data
 */
void dbg_breakpoint_set_original_data(uint64_t offset, uint8_t data)
{

	dbg_bp *ptr = breakpoints;
	while (ptr) {
		if (ptr->addr == offset + g_baddr) {
			ptr->orig_data = data;
			break;
		}
		ptr = ptr->next;
	}
}

void dbg_break_handle(uint64_t rip)
{
	dbg_bp *ptr = breakpoints;
	while (ptr) {
		if (ptr->addr == rip - 1) {
			break;
		}
		ptr = ptr->next;
	}
	if (!ptr) {
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, "WARNING: Expected a breakpoint but there was none heh!\n");
#endif
		return;
	}
#ifdef DEBUG_DEBUGGER
	fprintf(stderr, "BreakPoint hit: 0x%016lx (0x%lx)\n", rip - 1, rip - g_baddr - 1);
#endif

	if (ptr->handler) {
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, ">>>>>> CALLING HANDLER 0x%lx!\n", ptr->handler);
#endif
		uint64_t (*handler_func)(uint64_t) = (uint64_t(*)(uint64_t))(g_baddr + ptr->handler);
		handler_func(rip - g_baddr - 1);
	}
	if (ptr->uhandler) {
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, ">>>>>> CALLING USER HANDLER '%s'!\n", ptr->uhandler);
#endif
		dbg_function_call(ptr->uhandler);
		if (ptr->calls != -1) {
			ptr->calls--;
			// fprintf(stderr, "%lx -> %ld\n", ptr->addr - g_baddr, ptr->calls);
		}
	}

	if (ptr->calls == 0) {
		// We reached the number 0, now just call the death handler and delete the breakpoint
		char dhandler[32] = { 0 };
		bool handle = false;
		if (ptr->dhandler) {
			strncpy(dhandler, ptr->dhandler, 32);
			handle = true;
		}

		dbg_breakpoint_delete(ptr->addr, true);

		if (handle) {
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, ">>>>>> CALLING DEATH HANDLER '%s'!\n", dhandler);
#endif
			dbg_function_call(dhandler);
		}

		// Before continuing, since we deleted the breakpoint, continue won't
		// do the 1 byte substraction to RIP, so let's just do it here manually
		struct user_regs_struct *regs = dbg_regs_get();
		regs->rip = regs->rip - 1;
		dbg_regs_set(regs);
		free(regs);
	}

#ifdef DEBUG_DEBUGGER
	fprintf(stderr, ">>>>>> Automatic continue after handling breakpoint!\n");
#endif
	dbg_continue(false);
}

/*
 * Tell the child process to continue.
 * Doesn't send any specific signal.
 */
void dbg_continue(bool restore)
{
	struct user_regs_struct regs;
	ptrace(PTRACE_GETREGS, g_pid, NULL, &regs);

	dbg_bp *ptr = breakpoints;
	while (ptr) {
		// fprintf(stderr, "Looking for bp: RIP, BPADDR: %llx, %lx\n", regs.rip, ptr->addr);
		if (ptr->addr == regs.rip - 1) {
			break;
		}
		ptr = ptr->next;
	}

	// We reached one of our breakpoints
	if (ptr) {
		// We executed an int3 execution
		// We have to roll back one byte before
		regs.rip -= 1;
		if (ptrace(PTRACE_SETREGS, g_pid, NULL, &regs) == -1) {
			dbg_die("Cannot set new registers value");
		}

		if (regs.rip != ptr->addr) {
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, "What? %llx %lx\n", regs.rip, ptr->addr);
#endif
			dbg_die("WHAT THE FUCK IS GOING ON?!");
		}

		// Replace the breakpoint by its original instruction
		long orig = ptrace(PTRACE_PEEKTEXT, g_pid, ptr->addr, NULL);
		if (orig == -1) {
			dbg_die("Cannot get the original instruction");
		}
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, " Resuming on instruction: %lx\n", orig);
#endif
		if ((orig & 0xFF) == 0xcc) {
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, "Restoring was forced, as we don't want to execute an interruption\n");
#endif
			restore = true;
		}
		if (restore) {
			long newdata = (orig & 0xffffffffffffff00) | ptr->orig_data;
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, " Restoring instruction:   %lx\n", newdata);
#endif
			if (ptrace(PTRACE_POKETEXT, g_pid, ptr->addr, newdata) == -1) {
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
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, "Unexpected signal received: %s\n", strsignal(WSTOPSIG(status)));
#endif
			dbg_die("WHAT THE FUCK WHY JOHN WHYYYYYYYY");
		}

		// Rewrite the breakpoint and continue
		if (ptrace(PTRACE_POKETEXT, g_pid, ptr->addr, orig) == -1) {
			dbg_die("Cannot modify the program instruction");
		}

#ifdef DEBUG_DEBUGGER
		fprintf(stderr, " Resuming at addr: 0x%llx (0x%llx)\n", regs.rip, regs.rip - g_baddr);
#endif
		if (ptrace(PTRACE_CONT, g_pid, NULL, NULL) == -1) {
			dbg_die("Cannot continue the process");
		}
	} else {
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, "Resuming at addr: 0x%llx (%llx) (Warning: unknown reason)\n", regs.rip, regs.rip - g_baddr);
#endif
		if (ptrace(PTRACE_CONT, g_pid, NULL, NULL) == -1) {
			dbg_die("Cannot continue the process");
		}
	}
}

/*
 * Read memory from the debuggee.
 * The returned buffer must be freed.
 */
uint8_t *dbg_mem_read(uint64_t offset, int nb_bytes)
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
		if (ret == 0xffffffffffffffff) {
			dbg_die("Reading memory failed!");
		}
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

uint8_t *dbg_mem_read_va(uint64_t addr, int nb_bytes)
{
	return dbg_mem_read(addr - g_baddr, nb_bytes);
}

void dbg_mem_xor(uint64_t offset, uint64_t val)
{
	uint8_t *mem = dbg_mem_read(offset, 8);
	uint64_t absmem = *((uint64_t *) mem);
	FREE(mem);

	// fprintf(stderr, "XORING: %lx -> ", absmem);
	absmem ^= val;
	// fprintf(stderr, "%lx\n", absmem);
	dbg_mem_write(offset, 8, (uint8_t *) &absmem);
}

/*
 * Write debuggee's memory.
 */
void dbg_mem_write(uint64_t offset, int nb_bytes, const uint8_t *data)
{
	void *addr = (void *) g_baddr + offset;
	unsigned long word;
	for (int i = 0; i < nb_bytes; i += sizeof(long)) {
		size_t diff = nb_bytes - i;
		if (diff < sizeof(long)) {
			uint8_t *old_data = dbg_mem_read(offset + i, sizeof(long));
			word = ((long *)old_data)[0];
			word >>= diff * sizeof(long);
			word <<= diff * sizeof(long);
			unsigned long mask = *((unsigned long *) (data + i));
			mask <<= (sizeof(long) - diff) * sizeof(long);
			mask >>= (sizeof(long) - diff) * sizeof(long);
			word |= mask;
			FREE(old_data);
		} else {
			word = *((unsigned long *) (data + i));
		}
		//uint8_t *fu = dbg_mem_read(offset + i, nb_bytes);
		//fprintf(stderr, "---------BEFORE WRITE: (%lx) %016lx\n", offset + i, *((uint64_t*)fu));
		//fprintf(stderr, "---------       WRITE:        %016lx\n", word);
		long ret = ptrace(PTRACE_POKETEXT, g_pid, addr + i, word);
		if (ret == -1) {
			dbg_die("Could not write to memory!");
		}
		//fu = dbg_mem_read(offset + i, nb_bytes);
		//fprintf(stderr, "----------AFTER WRITE: (%lx) %016lx\n", offset + i, *((uint64_t*)fu));
	}
}

void dbg_mem_write_va(uint64_t va, int nb_bytes, const uint8_t *data)
{
	dbg_mem_write((int) (va - g_baddr), nb_bytes, data);
}

void dbg_mem_copy(uint64_t dest, uint64_t size, uint64_t from)
{
	uint8_t *mem = dbg_mem_read(from, size);
	// fprintf(stderr, "Copying %d bytes from here -> %lx\n", size, (uint64_t)*mem);
	dbg_mem_write(dest, size, mem);
	FREE(mem);
}

void dbg_mem_show(int offset, int len)
{
	uint8_t *mem = dbg_mem_read(offset, len);
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
	FREE(mem);
}

struct user_regs_struct *dbg_regs_get(void)
{
	struct user_regs_struct *regs = (struct user_regs_struct *) malloc(sizeof(struct user_regs_struct));
	ptrace(PTRACE_GETREGS, g_pid, NULL, regs);
	return regs;
}

void dbg_regs_set(struct user_regs_struct *regs)
{
	long ret = ptrace(PTRACE_SETREGS, g_pid, NULL, regs);
	if (ret == -1) {
		dbg_die("Could not set registers!");
	}
}

void dbg_regs_flag_reverse(char flag)
{
	struct user_regs_struct* regs = dbg_regs_get();
	if (flag == 'z') {
		regs->eflags = regs->eflags ^ 0x40;
	}
	dbg_regs_set(regs);
	FREE(regs);
}

uint64_t dbg_regs_get_val(const char *reg)
{
	struct user_regs_struct *regs = dbg_regs_get();
	uint64_t val = 0;
	if (!strcmp(reg, "eax")) {
		val = regs->rax & 0xffffffff;
	} else if (!strcmp(reg, "ebx")) {
		val = regs->rbx & 0xffffffff;
	} else if (!strcmp(reg, "ecx")) {
		val = regs->rcx & 0xffffffff;
	} else if (!strcmp(reg, "edx")) {
		val = regs->rdx & 0xffffffff;
	} else if (!strcmp(reg, "edi")) {
		val = regs->rdi & 0xffffffff;
	} else if (!strcmp(reg, "esi")) {
		val = regs->rsi & 0xffffffff;
	} else if (!strcmp(reg, "ebp")) {
		val = regs->rbp & 0xffffffff;
	} else if (!strcmp(reg, "esp")) {
		val = regs->rsp & 0xffffffff;
	} else if (!strcmp(reg, "eip")) {
		val = regs->rip & 0xffffffff;

	} else if (!strcmp(reg, "rax")) {
		val = regs->rax;
	} else if (!strcmp(reg, "rbx")) {
		val = regs->rbx;
	} else if (!strcmp(reg, "rcx")) {
		val = regs->rcx;
	} else if (!strcmp(reg, "rdx")) {
		val = regs->rdx;
	} else if (!strcmp(reg, "rdi")) {
		val = regs->rdi;
	} else if (!strcmp(reg, "rsi")) {
		val = regs->rsi;
	} else if (!strcmp(reg, "rbp")) {
		val = regs->rbp;
	} else if (!strcmp(reg, "rsp")) {
		val = regs->rsp;
	} else if (!strcmp(reg, "rip")) {
		val = regs->rip;
	}
	FREE(regs);

	return val;
}

void dbg_regs_set_val(const char *reg, uint64_t newval)
{
	struct user_regs_struct *regs = dbg_regs_get();
	if (!strcmp(reg, "eax") || !strcmp(reg, "rax")) {
		regs->rax = newval;
	} else if (!strcmp(reg, "ebx") || !strcmp(reg, "rbx")) {
		regs->rbx = newval;
	} else if (!strcmp(reg, "ecx") || !strcmp(reg, "rcx")) {
		regs->rcx = newval;
	} else if (!strcmp(reg, "edx") || !strcmp(reg, "rdx")) {
		regs->rdx = newval;
	} else if (!strcmp(reg, "edi") || !strcmp(reg, "rdi")) {
		regs->rdi = newval;
	} else if (!strcmp(reg, "ebp") || !strcmp(reg, "rbp")) {
		regs->rbp = newval;
	} else if (!strcmp(reg, "esi") || !strcmp(reg, "rsi")) {
		regs->rsi = newval;
	} else if (!strcmp(reg, "esp") || !strcmp(reg, "rsp")) {
		regs->rsp = newval;
	} else if (!strcmp(reg, "eip") || !strcmp(reg, "rip")) {
		regs->rip = newval;
	}
	dbg_regs_set(regs);
	FREE(regs);
}

bool dbg_function_register(const char* firstline, FILE *fileptr)
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
			FREE(line);
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

void dbg_function_call(const char *uhandler)
{
	dbg_function *fptr = functions;
	while (fptr) {
		// Execute the function
		if (!strcmp(uhandler, fptr->name)) {
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, "Executing Func: %s\n", fptr->name);
#endif
			dbg_function_line *line = fptr->firstline;
			while (line) {
#ifdef DEBUG_DEBUGGER
				fprintf(stderr, "  -> %s", line->line);
#endif
				dbg_parse_command(line->line);
				line = line->next;
			}
			break;
		}

		// Switch to next function
		fptr = fptr->next;
	}
}

void dbg_action_ret()
{
	struct user_regs_struct *regs = dbg_regs_get();
	uint8_t *ptr = dbg_mem_read_va(regs->rsp, 8);
	regs->rsp += 8;
	regs->rip = *((uint64_t *)ptr);
	dbg_regs_set(regs);
	FREE(ptr);
	FREE(regs);
}

inline uint64_t dbg_va_to_offset(uint64_t va)
{
	return va - g_baddr;
}
