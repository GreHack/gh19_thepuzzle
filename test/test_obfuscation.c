#include <stdio.h>
#include <stdlib.h>
#include "global.h"
#include <string.h>

//#undef TUPAC_BEG
//#undef TUPAC_END
//#define TUPAC_BEG
//#define TUPAC_END

int try_me(int n)
{
	TUPAC_BEG
	unsigned int val = 0;
	val = val + 10;
	val = val - 10;
	if (val == 0xdeadbeef) {
		val += 0x1000;
	}
	if (n < -10) {
		val++;
	} else if (n < -100) {
		val += 100;
	} else if (n > 123123123) {
		val -= 1000000;
	}
	if (val == 0xdeadc0de) {
		val += 0x100;
	}
	if (val == 0xc0febabe) {
		val += 0x100;
	}
	if (val == 0x00031337) {
		val += 0x1337;
	}
	if (val == 0x31337000) {
		val += 0x1337;
	}
	TUPAC_END
	return val;
}

void useless_padding(int a)
{
	int x = 20 + a;
	x *= a;
}

int recursive_function(int a)
{
	//TUPAC_BEG
	int res = 1;
	if (!a) {
		res = recursive_function(a + 1);
	}
	//TUPAC_END
	return res;
}

int another_function(int total)
{
	TUPAC_BEG
	int r = total % 5;
	int val = try_me(total + r);
	switch (r) {
	case 0:
		val += 1;
		break;
	case 1:
		val += recursive_function(0);
		break;
	case 2:
		val += 4;
		free(NULL);
		break;
	case 3:
		val += 1;
		(void) strcmp("hello", "waht");
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
		char *fu = malloc(30);
		if (fu) {
			strcpy(fu, "Well I just want to try this!");
			fu[29] = 0;
			total += fu[3];
		}
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
