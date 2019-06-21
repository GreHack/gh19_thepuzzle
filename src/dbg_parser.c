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
	OPAR,
	CPAR,
	REG,
	END
} token;

const char *token_name[END+1] = {
	"INT",
	"PLUS",
	"MINUS",
	"MULT",
	"OPAR",
	"CPAR",
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
			break;
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
		case '(':
			++p;
			return OPAR;
		case ')':
			++p;
			return CPAR;
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
 * Level 0 general expression, high priority
 */
static void dbg_parse_expr0(uint64_t *r)
{
	switch (CURRENT_TOKEN) {
	case INT:
		*r = att;
		parse_token(INT);
		break;
	case REG:
		*r = att;
		parse_token(REG);
		break;
	default:
		dbg_die("Invalid token for tokexpr");
	}
}

/*
 * Level 1 expression, normal priority
 */
static void dbg_parse_expr1X(uint64_t v, uint64_t *r)
{
	uint64_t n;
	switch (CURRENT_TOKEN) {
	case MULT:
		parse_token(MULT);
		dbg_parse_expr0(&n);
		dbg_parse_expr1X(v * n, r);
		break;
	default:
		// Epsilon, do nothing
		*r = v;
		break;
	}
}

/*
 * Level 1 general expression
 */
static void dbg_parse_expr1(uint64_t *r)
{
	uint64_t n;
	dbg_parse_expr0(&n);
	dbg_parse_expr1X(n, r);
}

/*
 * Level 2 expression, lower priority
 * v is the previous value (coming from the left)
 */
static void dbg_parse_expr2X(uint64_t v, uint64_t *r)
{
	uint64_t n;
	switch(CURRENT_TOKEN) {
	case PLUS:
		parse_token(PLUS);
		dbg_parse_expr1(&n);
		dbg_parse_expr2X(v + n, r);
		break;
	case MINUS:
		parse_token(MINUS);
		dbg_parse_expr1(&n);
		dbg_parse_expr2X(v - n, r);
	default:
		// Epsilon, do nothing
		*r = v;
		break;
	}
}

/*
 * General expression, can be anything
 */
static void dbg_parse_expr(uint64_t *r)
{
	uint64_t n;
	dbg_parse_expr1(&n);
	dbg_parse_expr2X(n, r);
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
		fprintf(stderr, "PARSING COMPLETE, result: %lu\n", res);
		// dbg_break(res);
	} else {
		dbg_die("I don't understand what you say bro");
	}
}
