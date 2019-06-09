#pragma once

#include <sys/ptrace.h>
#include <stdlib.h>
#include <signal.h>

void dbg_attach(int pid);
long dbg_break(void *addr);
void dbg_continue();

