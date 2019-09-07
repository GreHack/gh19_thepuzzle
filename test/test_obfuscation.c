#include <stdio.h>
#include <stdlib.h>
#include "global.h"

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
		break;
	case 3:
		val += 1;
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
	}
	TUPAC_END
	return total;
}

void obfuscation_main()
{
	TUPAC_BEG
	int result = do_test();
	fprintf(stderr, "%d\n", result);
	TUPAC_END
	exit(0);
}
