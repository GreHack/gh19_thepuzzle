#include "dbg.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "global.h"

static int g_pid = -31337; // Hack because I don't want to rewrite dbg_die
static char last_reg[8] = { 0 };

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

static inline int is_space(const char p)
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
	int i = 0;
	while (!is_space(*p) && *p != '\0' && i < sizeof(last_reg)) {
		last_reg[i] = *p;
		++p;
		i++;
	}
	last_reg[i] = '\0';

	att = dbg_regs_get_val(last_reg);
}

token dbg_token_next()
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
#ifdef DEBUG_DEBUGGER_PARSER
		fprintf(stderr, "Expected %s but found %s ...\n",
				token_name[expected], token_name[CURRENT_TOKEN]);
#endif
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
#ifdef DEBUG_DEBUGGER_PARSER
		fprintf(stderr, "Got invalid token for tokexpr: %s\n", token_name[CURRENT_TOKEN]);
#endif
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
void dbg_parse_expr(uint64_t *r)
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
	if (!strncmp(word, "b", len)) {
		// Break command takes two arguments
		uint64_t bp_addr = 0;
		uint64_t bp_handler = 0;
		dbg_parse_expr(&bp_addr);
		dbg_parse_expr(&bp_handler);
		dbg_breakpoint_add_handler((void *) bp_addr, (void *) bp_handler, NULL, NULL);
		//fprintf(stderr, "Adding bp at: 0x%lx (handler: 0x%lx)\n", bp_addr, bp_handler);
	}
	else if (!strncmp(word, "bh", len)) {
		// Break command takes two or three arguments
		uint64_t bp_addr = 0;
		dbg_parse_expr(&bp_addr);
		while (*p == ' ') {
			p++;
		}
		char handler_name[32] = { 0 };
		int i = 0;
		while (!is_space(*p) && *p != '\0') {
			handler_name[i] = *p;
			i++;
			p++;
		}
		while (*p == ' ') {
			p++;
		}
		char death_handler_name[32] = { 0 };
		i = 0;
		while (!is_space(*p) && *p != '\0') {
			death_handler_name[i] = *p;
			i++;
			p++;
		}
		// fprintf(stderr, "Adding bp at: 0x%lx (handler: '%s', death_handler: '%s')\n", bp_addr, handler_name, death_handler_name);
		dbg_breakpoint_add_handler((void *) bp_addr, NULL, handler_name, death_handler_name);
	}
	else if (!strncmp(word, "c", len)) {
		dbg_continue(true);
	}
	else if (!strncmp(word, "w", len)) {
		// Break command takes two arguments
		uint64_t offset = 0;
		uint64_t what = 0;
		uint64_t size = 0;
		dbg_parse_expr(&offset);
		dbg_parse_expr(&what);
		dbg_parse_expr(&size);
		// fprintf(stderr, "Parsed: %lx %lx %ld\n", offset, what, size);
		if (size == 0) {
			// Automatically detect the size
			uint64_t tmp = what;
			while (tmp) {
				tmp >>= 8;
				size++;
			}
		}
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, "Write mem at %lx (%ld) (%lx)\n", offset, size, what);
#endif
		dbg_mem_write(offset, size, (uint8_t*) &what);
	}
	else if (!strncmp(word, "wr", len)) {
		// Write n random bytes at destination
		uint64_t location = 0;
		uint64_t size = 0;
		dbg_parse_expr(&location);
		dbg_parse_expr(&size);
		uint8_t *what = malloc(size);
		for (uint64_t i = 0; i < size; i++) {
			what[i] = rand() % 256;
		}
		dbg_mem_write(location, size, what);
		free(what);
	}
	else if (!strncmp(word, "f", len)) {
		// Argument one is the target flag
		char flag = ptr[1];
		dbg_regs_flag_reverse(flag);
	}
	else if (!strncmp(word, "a", len)) {
		// Add a value to a register
		uint64_t reg_val = 0;
		uint64_t val_to_add = 0;
		dbg_parse_expr(&reg_val);
		dbg_parse_expr(&val_to_add);
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, "Val to add: %lx + %lx (new val: %lx)\n", reg_val, val_to_add, reg_val + val_to_add);
#endif
		dbg_regs_set_val(last_reg, reg_val + val_to_add);
		memset(last_reg, 0, sizeof(last_reg));
	}
	else if (!strncmp(word, "x", len)) {
		// Let's xor with 1 the byte at this offset
		uint64_t offset = 0;
		dbg_parse_expr(&offset);
		dbg_mem_xor(offset, 1);
	}
	else if (!strncmp(word, "n", len)) { // I don't know why 'n' but c was already taken
		// Copy n bytes from memory to memory location
		uint64_t dest = 0;
		uint64_t from = 0;
		uint64_t size = 0;
		dbg_parse_expr(&dest);
		dbg_parse_expr(&from);
		dbg_parse_expr(&size);

		dbg_mem_copy(dest, size, from);
	}
	else {
		dbg_die("I don't understand what you say bro");
	}
}

void dbg_parse_script(char *script, char* key, unsigned int key_len)
{
	char *line = NULL;
	size_t nb = 0;
	/* open script file */
	FILE *script_fd = fopen(script, "r");
	if (!script_fd) {
		return;
	}
	if (key && key_len > 0) {
		/* get file size */
		unsigned int script_sz;
		fseek(script_fd, 0, SEEK_END);
		script_sz = ftell(script_fd);
		fseek(script_fd, 0, SEEK_SET);
		/* read and decrypt script */
		char *script_content_enc = (char *) malloc(script_sz);
		fread(script_content_enc, script_sz, 1, script_fd);
		char *script_content = rc4_crypt(key, key_len, script_content_enc, script_sz);
		/* free useless structures */
		fclose(script_fd);
		FREE(script_content_enc);
		/* hack to use getline from libc on script content */
		char tmp_name[32] = "/tmp/XXXXXX";
		script_fd = fdopen(mkstemp(tmp_name), "r+");
		fwrite(script_content, script_sz, 1, script_fd);
		fseek(script_fd, 0, SEEK_SET);
		FREE(script_content);
	}

	const char func_begin[] = "begin ";

	/* Read file content line by line */
	while (getline(&line, &nb, script_fd) != -1) {
#ifdef DEBUG_DEBUGGER
		fprintf(stderr, "%s", line);
#endif
		if (!strncmp(func_begin, line, sizeof(func_begin) - 1)) {
			if (dbg_function_register(line, script_fd)) {
				continue;
			} else {
				dbg_die("Err... Error while parsing script!\n");
			}
		}
		dbg_parse_command(line);
		if (line) {
			FREE(line);
			line = NULL;
			nb = 0;
		}
	}
	fclose(script_fd);
	return;
}
