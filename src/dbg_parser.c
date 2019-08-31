#include "dbg.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int g_pid = -31337; // Hack because I don't want to rewrite dbg_die

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

#ifdef TEST
token dbg_token_next()
#else
static token dbg_token_next()
#endif
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
	// fprintf(stderr, "Read: %s\n", token_name[CURRENT_TOKEN]);
}

/*
 * Level 0 general expression, high priority
 */
static void dbg_parse_expr0(uint64_t *r)
{
	uint64_t n;
	switch (CURRENT_TOKEN) {
	case INT:
		*r = att;
		parse_token(INT);
		break;
	case REG:
		*r = att;
		parse_token(REG);
		break;
	case MINUS:
		parse_token(MINUS);
		dbg_parse_expr0(&n);
		*r = n * -1;
		break;
	case OPAR:
		parse_token(OPAR);
		dbg_parse_expr(r);
		parse_token(CPAR);
		break;
	case END:
		*r = 0;
		break;
	default:
		fprintf(stderr, "Got invalid token for tokexpr: %s\n", token_name[CURRENT_TOKEN]);
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
		break;
	default:
		// Epsilon, do nothing
		*r = v;
		break;
	}
}

/*
 * General expression, can be anything
 */
#ifndef TEST
static void dbg_parse_expr(uint64_t *r)
#else
void dbg_parse_expr(uint64_t *r)
#endif
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

	//fprintf(stderr, "Handling line: %s\n", input);
	while (!is_space(*ptr) && *ptr != '\0') {
		ptr++;
	}
	len = ptr - input;
	word = calloc(1, len + 1);
	if (!word) {
		dbg_die("Cannot parse user input!");
	}
	memcpy(word, input, len);

	// Init our parser because any command will use it
	p = ptr;
	CURRENT_TOKEN = dbg_token_next();
	// fprintf(stderr, "Read (init): %s\n", token_name[CURRENT_TOKEN]);

	// Get the command
	// TODO The long name joke is probably not a good idea, wdyt?
	if (!strncmp(word, "CreateBreakpointAtAddress", len) ||
			!strncmp(word, "b", len)) {
		// Break command takes two arguments
		uint64_t bp_addr = 0;
		uint64_t bp_handler = 0;
		dbg_parse_expr(&bp_addr);
		dbg_parse_expr(&bp_handler);
		dbg_breakpoint_add_handler((void *) bp_addr, (void *) bp_handler, NULL);
		//fprintf(stderr, "Adding bp at: 0x%lx (handler: 0x%lx)\n", bp_addr, bp_handler);
	}
	else if (!strncmp(word, "CreateBreakpointAtAddressWithHandler", len) ||
			!strncmp(word, "bh", len)) {
		// Break command takes two arguments
		uint64_t bp_addr = 0;
		dbg_parse_expr(&bp_addr);
		char handler_name[32];
		strncpy(handler_name, p, sizeof(handler_name));
		dbg_breakpoint_add_handler((void *) bp_addr, NULL, handler_name);
		//fprintf(stderr, "Adding bp at: 0x%lx (handler: User defined function '%s'\n", bp_addr, handler_name);
	}
	else if (!strncmp(word, "ContinueProcess", len) ||
			!strncmp(word, "c", len)) {
		dbg_continue(true);
	}
	else if (!strncmp(word, "WriteMemoryAt", len) ||
			!strncmp(word, "w", len)) {
		// Break command takes two arguments
		uint64_t offset = 0;
		uint64_t what = 0;
		dbg_parse_expr(&offset);
		dbg_parse_expr(&what);
		// TODO 1 is wrong here and &what as well
		dbg_mem_write(offset, 1, &what);
	}
	else {
		dbg_die("I don't understand what you say bro");
	}
}

void dbg_parse_script(char *script) {
	char *line = NULL;
	size_t nb = 0;
	/* Open script file */
	FILE *script_fd = fopen(script, "r");
	if (!script_fd) {
		return;
	}

	const char func_begin[] = "begin ";

	/* Read file content line by line */
	while (getline(&line, &nb, script_fd) != -1) {
		if (!strncmp(func_begin, line, sizeof(func_begin) - 1)) {
			if (dbg_function_register(line, script_fd)) {
				continue;
			} else {
				dbg_die("Err... Error while parsing script!\n");
			}
		}
		dbg_parse_command(line);
		if (line) {
			free(line);
			line = NULL;
			nb = 0;
		}
	}
	fclose(script_fd);
	return;
}
