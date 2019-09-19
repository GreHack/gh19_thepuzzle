#ifndef __DBG_H__
#define __DBG_H__

#include <sys/ptrace.h>
#include <sys/user.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>

#ifdef DEBUG_DEBUGGER
#define dbg_die(x) { \
	printf("==== FATAL: DBG DEAD -- %s - %s:%d\n", x, __FILE__, __LINE__); \
	perror("Reason:"); \
	kill(g_pid, SIGKILL); \
	exit(-1); \
	}
#else
#define dbg_die(x) { kill(g_pid, SIGKILL); exit(-1); }
#endif


/* dbg.c */
void dbg_attach(int pid);

void dbg_breakpoint_add(void *addr);
void dbg_breakpoint_add_handler(void *addr, void *handler, const char *uhandler, const char *dhandler);
void dbg_breakpoint_delete(uint64_t addr, bool restore);
void dbg_breakpoint_delete_off(uint64_t offset, bool restore);
void dbg_breakpoint_disable(uint64_t offset, uint64_t size);
void dbg_breakpoint_enable(uint64_t offset, uint64_t size, bool restore_original);
void dbg_breakpoint_set_original_data(uint64_t offset, uint8_t data);

void dbg_break_handle(uint64_t rip);

void dbg_continue(bool restore);

uint8_t *dbg_mem_read(uint64_t offset, int nb_bytes);
uint8_t *dbg_mem_read_va(uint64_t addr, int nb_bytes);
void dbg_mem_write(uint64_t offset, int nb_bytes, const uint8_t *data);
void dbg_mem_write_va(uint64_t va, int nb_bytes, const uint8_t *data);
void dbg_mem_show(int offset, int len);
void dbg_mem_xor(uint64_t offset, uint64_t val);

struct user_regs_struct *dbg_regs_get(void);
void dbg_regs_set(struct user_regs_struct *regs);
void dbg_regs_flag_reverse(char flag);
uint64_t dbg_regs_get_val(const char *reg);
void dbg_regs_set_val(const char *reg, uint64_t newval);

bool dbg_function_register(const char* firstline, FILE *fileptr);
void dbg_function_call(const char *uhandler);

// Simulate the ret instruction
void dbg_action_ret();

// Useful function for debugging outside dbg.c
uint64_t dbg_va_to_offset(uint64_t va);


/* dbg_parser.c */
void dbg_parse_command(const char* input);

typedef enum token_t {
	INT,
	PLUS,
	MINUS,
	MULT,
	OPAR,
	CPAR,
	REG,
	END
} token;

token CURRENT_TOKEN;
const char* p; // Current position
uint64_t att; // Current token attribute

void dbg_parse_expr(uint64_t *r);
#ifdef TEST
 // Expose those values for testing
 extern token CURRENT_TOKEN;
 token dbg_token_next();
 const char* token_name[END+1];
#endif

void dbg_parse_script(char *script, char *key, unsigned int key_len);

#endif
