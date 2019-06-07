#include "core.h"
#include "dbg.h"
#include <stdio.h>

void child(int father_pid)
{
	int c;

	for (int i = 0; i < 3; i++) {
		printf("Hello %d!\n", i);
		sleep(1);
	}

	// 0x000014a1      8b45ec         mov eax, dword [rbp - 0x14]
	c = father_pid / 2;

	for (;;) {
		printf("Sleeping forever!\n");
		sleep(1);
	}
}

void father(int child_pid)
{
	int stat_loc;
	long err;

	// Attach to our child
	dbg_attach(child_pid);

	// Add a breakpoint
	dbg_break((void *) 0x14a1);

	// Wait to catch our breakpoint
	// TODO Fix those waitpid
	waitpid(child_pid, &stat_loc, 0);

	printf("Wait (1) ok.\n");

	// Wait that the process is dead
	// waitpid(child_pid, &stat_loc, 0);
	printf("Wait (X) ok. Exiting.\n");
}

int main(int argc)
{
	int father_pid = getpid();

	int child_pid = fork();
	if (!child_pid) {
		child(father_pid);
	} else {
		printf("Father created: %d, child is: %d\n", father_pid, child_pid);
		father(child_pid);
	}

	return 0;
}
