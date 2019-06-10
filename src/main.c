#include "core.h"
#include "dbg.h"
#include "unpack.h"
#include "packed/hello.h"
#include <stdio.h>
#include <string.h>

// Our breakpoints locations
// TODO Find a way to automate this?
// 0x00001547      488d3dbd0b00.  lea rdi, str.Sleeping_forever
// 0f5e is the entry point of the function check_flag
#define BP_CHILD_01 0x1512

/*
 * This is the debuggee, our child process.
 */
void child(char *flag, int len)
{
	for (int i = 0; i < 1; i++) {
		printf("Hello %d!\n", i);
		sleep(1);
	}

	check_flag(flag, len);
}

/*
 * This is our debugger, the "father" program.
 * Because of yama hardening, the father *must* be our debugger.
 * Check /proc/sys/kernel/yama/ptrace_scope
 */
void father(int child_pid)
{
	int status;
	long err;
	struct user_regs_struct *regs;

	// Attach to our child
	dbg_attach(child_pid);

	// Add a breakpoint
	dbg_break((void *) BP_CHILD_01);

	// Wait to catch the first SIGSTOP sent by dbg_attach
	waitpid(child_pid, &status, 0);

	// Tell the process to continue
	dbg_continue();

	// Wait that the process is dead
	printf("Waiting...\n");
	waitpid(child_pid, &status, 0);
	// Get registr values
	regs = dbg_get_regs();
	unpack(child_pid, BP_CHILD_01);
	// Tell the process to continue
	regs->rip -= 1;
	dbg_set_regs(regs);
	free(regs);
	dbg_continue();
	if (WIFEXITED(status)) {
		printf("exited, status=%d\n", WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		printf("killed by signal %d\n", WTERMSIG(status));
	} else if (WIFSTOPPED(status)) {
		printf("stopped by signal %d\n", WSTOPSIG(status));
	} else if (WIFCONTINUED(status)) {
    		printf("continued\n");
	}
	printf("Wait ok (status %i). Exiting.\n", status);
	// TODO Check waitpid status and don't exit when the child reached
	// a breakpoint (add some main loop)
}

int main(int argc, char **argv)
{
	int father_pid = getpid();
	
	if (argc < 2) {
		fprintf(stderr, "usage: ./%s FLAG\n-- aborting\n", argv[0]);
		exit(1);
	}

	int child_pid = fork();
	if (!child_pid) {
		child(argv[1], strlen(argv[1]));
	} else {
		printf("Father created: %d, child is: %d\n", father_pid, child_pid);
		father(child_pid);
	}

	return 0;
}
