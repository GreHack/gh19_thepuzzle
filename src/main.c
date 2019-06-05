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

	// 0x000012e7      8b45ec         mov eax, dword [rbp - 0x14]
	// 0x000012ea      89c2           mov edx, eax               
	// 0x000012ec      c1ea1f         shr edx, 0x1f              
	// 0x000012ef      01d0           add eax, edx               
	// 0x000012f1      d1f8           sar eax, 1                 
	c = father_pid / 2;

	for (;;) {
		printf("Sleeping forever!");
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
	// Note: The && syntax to refer to a label is only valid with GCC
	dbg_break((void *) 0x12e7);

	// Wait to catch our breakpoint
	waitpid(child_pid, &stat_loc, 0);
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
