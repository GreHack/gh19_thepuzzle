#include "core.h"
#include "dbg.h"
#include "unpack.h"
#include "packed/hello.h"
#include <stdio.h>
#include <string.h>

// Our breakpoints locations
// TODO Find a way to automate this
// 21d9 is the entry point of the function check_flag
#define BP_CHILD_01 0x12d7

/*
 * This is the debuggee, our child process.
 */
void child(char *flag, int len)
{
	for (int i = 0; i < 2; i++) {
		printf("Hello %d!\n", i);
		usleep(100);
	}

	printf("Break here please!\n");

	if (check_flag(flag, len))
		printf("congrats, I guess...\n");
	else
		printf("nope...\n");
}

/*
 * This is our debugger, the "father" program.
 * Because of yama hardening, the father *must* be our debugger.
 * Check /proc/sys/kernel/yama/ptrace_scope
 */
void father(int child_pid)
{
	int status;
	struct user_regs_struct *regs;

	dbg_attach(child_pid);

	// Wait to catch the first SIGSTOP sent by dbg_attach
	waitpid(child_pid, &status, 0);

	dbg_break((void *) BP_CHILD_01);

	dbg_continue();

	// Wait that the process is dead
	printf("Waiting...\n");
	waitpid(child_pid, &status, 0);

	//unpack(child_pid, BP_CHILD_01);

	// Tell the process to continue
	dbg_continue();
	regs = dbg_get_regs();
	if (WIFEXITED(status)) {
		printf("exited, status=%d\n", WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		printf("killed by signal %d\n", WTERMSIG(status));
	} else if (WIFSTOPPED(status)) {
		printf("stopped by signal %d\n", WSTOPSIG(status));
	} else if (WIFCONTINUED(status)) {
    		printf("continued\n");
	}
	printf("Wait ok (status %i | rip: %llx). Exiting.\n", status, regs->rip);
	free(regs);
}

int main(int argc, char **argv)
{
	int father_pid = getpid();
	char *input = malloc(1024);
	fgets(input, 1024, stdin);
	dbg_parse_command(input);
	return 0;
	
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
