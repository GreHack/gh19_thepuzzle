#include "core.h"
#include "dbg.h"
#include "screen.h"
#include "unpack.h"
#include "packed/ocr.h"
#include <stdio.h>
#include <string.h>

#if 0
void useless_function()
{
	int c = 1 + 3;
}
#endif

/*
 * This is the debuggee, our child process.
 */
void child(char *flag, int len)
{
	img_t *screenshot = screen_capture();
#ifdef DEBUG_MAIN
	img_to_file(screenshot, "/tmp/out.ppm");
#endif
	ocr_t *ocr = ocr_train("data/ocr/labels.bin", "data/ocr/data.bin");
#if DEBUG_MAIN
	fprintf(stderr, "user input: %s\n", ocr_read_flag(ocr, screenshot));
#endif
#if 0
	useless_function();
	if (check_flag(flag, len))
		printf("congrats, I guess...\n");
	else
		printf("nope...\n");
#endif
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
			} else if (sig == SIGSEGV) {
				fprintf(stderr, "Sorry, but it looks like there was a segfault here!\n");
				exit(1);
			} else {
				fprintf(stderr, "Unhandled signal!\n");
				exit(1);
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
	if (argc < 2) {
		fprintf(stderr, "usage: ./%s /path/to/script.debugging_script FLAG -- aborting\n", argv[0]);
		exit(1);
	}

	int father_pid = getpid();

	if (strncmp(".debugging_script", argv[1] + (strlen(argv[1]) - 17), 17) != 0) {
		fprintf(stderr, "unknown extension for first argument -- aborting\n");
		exit(1);
	}

	int child_pid = fork();
	if (!child_pid) {
		/* Wait to leave some time to attach */
		for (int i = 0; i < 2; i++) {
#if DEBUG_MAIN
			fprintf(stderr, "Hello %d!\n", i);
#endif
			usleep(100);
		}
		child(argv[2], strlen(argv[2]));
	} else {
#if DEBUG_MAIN
		fprintf(stderr, "Father created: %d, child is: %d\n", father_pid, child_pid);
#endif
		father(child_pid, argv[1]);
	}

	return 0;
}
