#include "dbg.h"
#include <stdio.h>

#define INPUT_SIZE 1024

int main()
{
	char *input = malloc(INPUT_SIZE);
	if (!input) {
		perror("Could not allocate memory");
		return 1;
	}

	setbuf(stdin, NULL);
	setbuf(stdout, NULL);

	while (fgets(input, INPUT_SIZE, stdin)) {
		uint64_t value;
		p = input;
		CURRENT_TOKEN = dbg_token_next();
		dbg_parse_expr(&value);
		printf("%lu\n", value);
	}

	return 0;
}
