#include "core.h"
#include "dbg.h"
#include "unpack.h"
#include "packed/hello.h"
#include <stdio.h>
#include <string.h>

// Our breakpoints locations
// TODO Find a way to automate this
// 21d9 is the entry point of the function check_flag
#define BP_CHILD_01 0x2a02

/*
 * This is the debuggee, our child process.
 */
void child(char *flag, int len)
{
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
void father(int child_pid, char *script_path)
{
	int status;
	struct user_regs_struct *regs;

	dbg_attach(child_pid);

	// Wait to catch the first SIGSTOP sent by dbg_attach
	waitpid(child_pid, &status, 0);

	/* Let's read the debugging file */
	dbg_parse_script(script_path);		

	// Main debugger loop
	while (true) {
		waitpid(child_pid, &status, 0);
		regs = dbg_get_regs();
		if (WIFEXITED(status)) {
			printf("exited, status=%d\n", WEXITSTATUS(status));
			break;
		} else if (WIFSIGNALED(status)) {
			printf("killed by signal %d\n", WTERMSIG(status));
			break;
		} else if (WIFSTOPPED(status)) {
			int sig = WSTOPSIG(status);
			printf("stopped by signal %d\n", sig);
			if (sig == SIGTRAP) {
				fprintf(stderr, "BP hit: %p\n", regs->rip);
				dbg_break_handle(regs->rip);
			}
		} else if (WIFCONTINUED(status)) {
			printf("continued\n");
		}
		printf("Wait ok (status %i | rip: %llx). Exiting.\n", status, regs->rip);
	}
	free(regs);
}

int main(int argc, char **argv)
{
	int father_pid = getpid();
	//char *input = malloc(1024);
	//fgets(input, 1024, stdin);
	//dbg_parse_command(input);
	//return 0;
	
	if (argc < 3) {
		fprintf(stderr, "usage: ./%s /path/to/script.debugging_script FLAG -- aborting\n", argv[0]);
		exit(1);
	}

	if (strncmp(".debugging_script", argv[1] + (strlen(argv[1]) - 17), 17) != 0) {
		fprintf(stderr, "unknown extension for first argument -- aborting\n");
		exit(1);
	}

	int child_pid = fork();
	if (!child_pid) {
		/* Wait to leave some time to attach */
		for (int i = 0; i < 2; i++) {
			printf("Hello %d!\n", i);
			usleep(100);
		}
		child(argv[2], strlen(argv[2]));
	} else {
		printf("Father created: %d, child is: %d\n", father_pid, child_pid);
		father(child_pid, argv[1]);
	}

	return 0;
}
