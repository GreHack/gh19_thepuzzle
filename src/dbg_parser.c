#include "core.h"
#include "dbg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int g_pid = -31337; // Hack because I don't want to rewrite dbg_die

typedef enum {
	INT,
	PLUS,
	MINUS,
	MULT,
	REG,
	END
} token;

const char *token_name[END+1] = {
	"INT",
	"PLUS",
	"MINUS",
	"MULT",
	"REG",
	"END"
};

static token CURRENT_TOKEN = -1;
static const char* p; // Current position
static uint64_t att; // Current token attribute

static int is_space(const char p)
{
	return p == ' ' || p == '\t' || p == '\n' || p == '\r';
}

static void dbg_parse_int()
{
	uint64_t res = 0;
	while (!is_space(*p)) {
		if (*p >= '0' && *p <= '9') {
			res = res * 10 + (*p - '0');
			++p;
		} else {
			dbg_die("Stop bullshitting me, invalid INT");
		}
	}
	att = res;
	//fprintf(stderr, "INT READ: %llu\n", res);
}

static void dbg_parse_reg()
{
	while (!is_space(*p) && *p != '\0') {
		++p;
	}

	// TODO Make sure the register is valid and get its value
	att = 0;
}

static token dbg_token_next()
{
	for (;;) {
		switch (*p) {
		case ' ': case '\t': case '\n': case '\r':
			++p;
			break;
		case '+':
			++p;
			return PLUS;
		case '-':
			++p;
			return MINUS;
		case '*':
			++p;
			return MULT;
		case '$':
			++p;
			dbg_parse_reg();
			return REG;
		case '\0':
			return END;
		default:
			dbg_parse_int();
			return INT;
		}
	}
}

static void parse_token(token expected)
{
	if (CURRENT_TOKEN != expected) {
		fprintf(stderr, "Expected %s but found %s ...\n",
				token_name[expected], token_name[CURRENT_TOKEN]);
		dbg_die("Unexpected token!");
	}
	if (CURRENT_TOKEN != END) {
		CURRENT_TOKEN = dbg_token_next();
	}
	fprintf(stderr, "Read: %s\n", token_name[CURRENT_TOKEN]);
}

/*
 * General expression, can be anything
 */
static void dbg_parse_expr(uint64_t *res)
{
	switch (CURRENT_TOKEN) {
	case INT:
		*res = att;
		parse_token(INT);
		break;
	case REG:
		parse_token(REG);
		break;
	default:
		dbg_die("Unsupported syntax");
	}
	// TODO Loop on itself
	// TODO Define the syntax...
}


void dbg_parse_command(const char* input)
{
	int len;
	const char *ptr = input;
	char *word = NULL;

	while (!is_space(*ptr) && *ptr != '\0') {
		ptr++;
	}
	len = ptr - input;
	word = calloc(1, len + 1);
	if (!word) {
		dbg_die("Cannot parse user input");
	}
	memcpy(word, input, len);

	// Init our parser because any command will use it
	p = ptr;
	CURRENT_TOKEN = dbg_token_next();
	uint64_t res = 0;
	fprintf(stderr, "Read (init): %s\n", token_name[CURRENT_TOKEN]);

	// Get the command
	if (!strncmp(word, "CreateBreakpointAtAddress", len) ||
			!strncmp(word, "b", len)) {
		dbg_parse_expr(&res);
		// dbg_break(res);
	} else {
		dbg_die("I don't understand what you say bro");
	}
}
