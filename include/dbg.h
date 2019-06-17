#ifndef __DBG_H__
#define __DBG_H__

#include <sys/ptrace.h>
#include <sys/user.h>
#include <stdlib.h>
#include <signal.h>

void dbg_attach(int pid);
void dbg_break(void *addr);
void dbg_continue();
char *dbg_read_mem(int offset, int nb_bytes);
void dbg_write_mem(int offset, int nb_bytes, char *data);
void dbg_show_mem(int offset, int len);
struct user_regs_struct *dbg_get_regs(void);
void dbg_set_regs(struct user_regs_struct *regs);

#endif
