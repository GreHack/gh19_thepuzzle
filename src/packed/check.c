
#include "packed/check.h"

#include <string.h>

bool check_flag(char *input)
{
	return 0 == strncmp(input, "hack", 4); 
}
