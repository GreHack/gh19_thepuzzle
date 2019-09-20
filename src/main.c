#include <stdio.h>
#include <string.h>

#include "global.h"
#include "core.h"
#include "dbg.h"
#ifndef TEST_OBFUSCATION
#include "unpack.h"
#include "screen.h"
#include "ocr.h"
#include "check.h"
#endif
#include "b64.h"

/*
 * These function try to hide the call to start_timer();
 * 2pac will add a breakpoint to tell the father to call start_timer()
 */
static int check_user(char *user)
{
	if (!strcmp(user, "root")) {
		return 1;
	}
	return 0;
}
static int hide_me()
{
	char *user = getenv("USER");
	int ret = check_user(user);
	return ret;
}

/*
 * This is the debuggee, our child process.
 */
void child(char *self_path)
{
	TUPAC_BEG
#ifdef TEST_OBFUSCATION

void obfuscation_main();
	(void)self_path;
	obfuscation_main();
	TUPAC_END
	return;

#else

	int r = hide_me();
	if (r) {
		fprintf(stderr, "Something looks wrong with your environment!\n");
		exit(1);
	}
	img_t *screenshot = screen_capture();
#ifdef DEBUG_MAIN
	img_to_file(screenshot, "/tmp/out.ppm");
#endif
#if KD_LOAD
	/* init RC4 stream */
	kd_rc4_state = rc4_init(KD_RC4_KEY, strlen(KD_RC4_KEY));
	FILE *fd = kd_get_fd(self_path);
	/* load kd tree */
	ocr_t *ocr = kd_load(fd);
#ifdef DEBUG_LOAD
	fprintf(stderr, "Load KD complete\n");
#endif
#else
	(void)self_path;
	ocr_t *ocr = ocr_train("data/ocr/labels.bin", "data/ocr/data.bin");
	FILE *fd = fopen("data/kd.bin", "w");
	kd_dump(ocr, fd);
#ifdef DEBUG_LOAD
	fprintf(stderr, "Dump KD complete\n");
#endif
#endif
	fclose(fd);
	char *input = ocr_read_flag(ocr, screenshot);
#if DEBUG_MAIN
	fprintf(stderr, "user input: %s\n", input);
#endif
	if (input) {
		if (check_flag(input))
			fprintf(stdout, "Congrats, you're a hacker!\n");
		else
			fprintf(stdout, "Nope. You think you're a hacker but you're not, go back to (Gh)id(r)a!\n");
	} else {
		fprintf(stdout, "Come on, give me some input to process, man.\n");
	}
	FREE(input);
#if KD_LOAD
	kd_free(ocr, true);
	rc4_free(kd_rc4_state);
#else
	entry_t **entries = ocr->entries;
	kd_free(ocr, false);
	if (entries)
		FREE(entries);
#endif
	img_free(screenshot);
	TUPAC_END
	return;
#endif
}

/*
 * This is our debugger, the "father" program.
 * Because of yama hardening, the father *must* be our debugger.
 * Check /proc/sys/kernel/yama/ptrace_scope
 */
void father(int child_pid, char *script_path, char *b64_key)
{
	int status = 0;
	struct user_regs_struct *regs = NULL;
	char *key = NULL;
	unsigned int key_len = 0;

	dbg_attach(child_pid);

	// Wait to catch the first SIGSTOP sent by dbg_attach
	waitpid(child_pid, &status, 0);

	if (b64_key) {
		/* get key from base64-encoded parameter */
		int length = strlen(b64_key);
		key = b64_decode(b64_key, length);
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, "key: %s (len: %u)\n", key, key_len);
#endif
		key_len = strlen(key);
	} else {
		key = NULL;
		key_len = 0;
	}

	/* Let's read the debugging file */
	dbg_parse_script(script_path, key, key_len);

	if (key) {
		FREE(key);
	}

	// Main debugger loop
	while (true) {
		waitpid(child_pid, &status, 0);
		regs = dbg_regs_get();
		if (WIFEXITED(status)) {
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, "Exited, status=%d\n", WEXITSTATUS(status));
#endif
			break;
		} else if (WIFSIGNALED(status)) {
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, "Killed by signal %d\n", WTERMSIG(status));
#endif
			break;
		} else if (WIFSTOPPED(status)) {
			int sig = WSTOPSIG(status);
#ifdef DEBUG_DEBUGGER
			fprintf(stderr, "Stopped by signal %d (%s): ", sig, strsignal(sig));
			fprintf(stderr, "%llx (%llx)\n", regs->rip, dbg_va_to_offset(regs->rip));
#endif
			if (sig == SIGTRAP) {
				dbg_break_handle(regs->rip);
			} else if (sig == SIGSEGV) {
				EXIT(1, "sorry, but it looks like there was a segfault here!")
			} else {
				EXIT(1, "unhandled signal")
			}
		} else if (WIFCONTINUED(status)) {
			fprintf(stderr, "Continued\n");
		}
		FREE(regs);
	}
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Usage: ./%s /path/to/ds/file [key] -- aborting\n", argv[0]);
		exit(1);
	}

	int father_pid = getpid();

	if (strncmp(".debugging_script", argv[1] + (strlen(argv[1]) - 17), 17) != 0) {
		fprintf(stderr, "Unknown extension for first argument -- aborting\n");
		exit(1);
	}

	if (argc >= 3 && strlen(argv[2]) != 4)
	{
		fprintf(stderr, "The key must meet our security requirements -- aborting");
		exit(1);
	}

	int child_pid = fork();
	if (!child_pid) {
		// TODO Is it funny or just a pity?
		/* Wait to leave some time to attach */
		for (int i = 0; i < 5; i++) {
#if DEBUG_MAIN
			fprintf(stderr, "Hello %d!\n", i);
#endif
			usleep(100);
		}
		child(argv[0]);
	} else {
#if DEBUG_MAIN
		fprintf(stderr, "Father created: %d, child is: %d\n", father_pid, child_pid);
#endif
		if (argc < 2) {
			father(child_pid, argv[1], NULL);
		} else {
			father(child_pid, argv[1], argv[2]);
		}
	}

	return 0;
}
