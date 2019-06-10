#include "core.h"
#include "dbg.h"
#include "packed/hello.h"
#include <stdio.h>
#include <string.h>

// Our breakpoints locations
// TODO Find a way to automate this?
// 0x00001547      488d3dbd0b00.  lea rdi, str.Sleeping_forever
#define BP_CHILD_01 0x1547

/*
 * This is the debuggee, our child process.
 */
void child(int father_pid)
{
	int c;

	for (int i = 0; i < 3; i++) {
		printf("Hello %d!\n", i);
		sleep(1);
	}

	c = father_pid / 2;

	for (;;) {
		printf("Sleeping forever!\n");
		sleep(1);
	}
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
	printf("Wait ok. Exiting.\n");
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
	
	if (check_flag(argv[1], strlen(argv[1])))
		printf("Yep, I guess you're right.\n");
	else
		printf("J'avais dit ravioli. La vie de ma mère.\n");
	exit(0);

	int child_pid = fork();
	if (!child_pid) {
		child(father_pid);
	} else {
		printf("Father created: %d, child is: %d\n", father_pid, child_pid);
		father(child_pid);
	}

	return 0;
}
