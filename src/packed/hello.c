
#include <stdbool.h>


char *FLAG = "AYO THIS IS THE FLAG\0";

bool check_flag(char *input, int len) {
	TUPAC_BEG
	int i = 0;
	bool res = true;
	for (i = 0; i < len; i++) {
		res &= (input[i] == FLAG[i]);
	}
	TUPAC_END
	return res;
}
