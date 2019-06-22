#ifndef __DBG_H__
#define __DBG_H__

#include <sys/ptrace.h>
#include <sys/user.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>


#define dbg_die(x) { \
	printf("==== FATAL: DBG DEAD -- %s - %s:%d\n", x, __FILE__, __LINE__); \
	perror("Reason:"); \
	kill(g_pid, SIGKILL); \
	exit(-1); \
	}


/* dbg.c */
void dbg_attach(int pid);
void dbg_break(void *addr);
void dbg_continue();
char *dbg_read_mem(int offset, int nb_bytes);
void dbg_write_mem(int offset, int nb_bytes, char *data);
void dbg_show_mem(int offset, int len);
struct user_regs_struct *dbg_get_regs(void);
void dbg_set_regs(struct user_regs_struct *regs);


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

#ifdef TEST
 // Expose those values for testing
 void dbg_parse_expr(uint64_t *r);
 extern token CURRENT_TOKEN;
 token dbg_token_next();
 const char* token_name[END+1];
#else
 static void dbg_parse_expr(uint64_t *r);
#endif

#endif
