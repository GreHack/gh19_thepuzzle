#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include <string.h>

int another_function(int total)
{
	TUPAC_BEG
	int r = total % 5;
	int val = 0;
	switch (r) {
	case 0:
		val += 1;
		break;
	case 1:
		val += 2;
		break;
	case 2:
		val += 4;
		free(NULL);
		break;
	case 3:
		val += 1;
		(void) strcmp("hello", "waht");
		unsigned char c = 0xeb;
		break;
	case 4:
		val += 3;
		break;
	}
	TUPAC_END
	return val;
}

int do_test()
{
	TUPAC_BEG
	int total = 0;
	for (int i = 0; i < 1337; i++) {
		total += another_function(total);
		char *fu = malloc(100);
		fu[0] = 1;
		free(fu);
	}
	TUPAC_END
	return total;
}

void obfuscation_main()
{
	int result = do_test();
	printf("%d\n", result);
	exit(0);
}
