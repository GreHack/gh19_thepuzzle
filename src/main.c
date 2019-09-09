#include <stdio.h>
#include <string.h>

#include "global.h"
#include "core.h"
#include "dbg.h"
#include "screen.h"
#include "unpack.h"
#include "packed/ocr.h"

/*
 * This is the debuggee, our child process.
 */
void child()
{
#ifdef TEST_OBFUSCATION

void obfuscation_main();
	obfuscation_main();

#else

	img_t *screenshot = screen_capture();
#ifdef DEBUG_MAIN
	img_to_file(screenshot, "/tmp/out.ppm");
#endif
	ocr_t *ocr = ocr_train("data/ocr/labels.bin", "data/ocr/data.bin");
	FILE *fd = fopen("/tmp/kd.bin", "w");
	kd_dump(ocr, fd);
#ifdef DEBUG_LOAD
	fprintf(stderr, "Dump KD complete\n");
#endif
	fclose(fd);
	fd = fopen("/tmp/kd.bin", "r");
	ocr = kd_load(fd);
#ifdef DEBUG_LOAD
	fprintf(stderr, "Load KD complete\n");
#endif
	fclose(fd);
	char *input = ocr_read_flag(ocr, screenshot);
#if DEBUG_MAIN
	fprintf(stderr, "user input: %s\n", input);
#endif
	FREE(input);
	return;
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
		regs = dbg_regs_get();
		if (WIFEXITED(status)) {
			printf("Exited, status=%d\n", WEXITSTATUS(status));
			break;
		} else if (WIFSIGNALED(status)) {
			printf("Killed by signal %d\n", WTERMSIG(status));
			break;
		} else if (WIFSTOPPED(status)) {
			int sig = WSTOPSIG(status);
#ifdef DEBUG_DEBUGGER
			printf("Stopped by signal %d (%s):\n", sig, strsignal(sig));
#endif
			if (sig == SIGTRAP) {
				dbg_break_handle(regs->rip);
			} else if (sig == SIGSEGV) {
				EXIT(1, "sorry, but it looks like there was a segfault here!")
			} else {
				EXIT(1, "unhandled signal")
			}
		} else if (WIFCONTINUED(status)) {
			printf("Continued\n");
		}
	}
	free(regs);
}

int main(int argc, char **argv)
{
	child();
	return 0;
	if (argc < 2) {
		fprintf(stderr, "Usage: ./%s /path/to/script.debugging_script -- aborting\n", argv[0]);
		exit(1);
	}

	int father_pid = getpid();

	if (strncmp(".debugging_script", argv[1] + (strlen(argv[1]) - 17), 17) != 0) {
		fprintf(stderr, "Unknown extension for first argument -- aborting\n");
		exit(1);
	}

	int child_pid = fork();
	if (!child_pid) {
		// TODO Is it funny or just a pity?
		/* Wait to leave some time to attach */
		for (int i = 0; i < 2; i++) {
#if DEBUG_MAIN
			fprintf(stderr, "Hello %d!\n", i);
#endif
			usleep(100);
		}
		child();
	} else {
#if DEBUG_MAIN
		fprintf(stderr, "Father created: %d, child is: %d\n", father_pid, child_pid);
#endif
		father(child_pid, argv[1]);
	}

	return 0;
}
