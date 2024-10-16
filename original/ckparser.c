/* ckparser.c -- the original Simply Parse in C code */

#include <stdio.h>
#include <string.h>

#include "ckparser.h"

/*
 * the source was copied and pasted from the web. both i and astyle
 * did some reformatting (my by hand changes noted below). changes
 * from original as found on web:
 *
 * * added forward declarations and created a header file for the
 *   public api.
 *
 * * reformat function prototypes and headers to left justify
 *   function names and put arguments on separate lines.
 *
 * * added variable names to prototypes that did not have them.
 *
 * * strip_right function was bugged, taking the wrong string
 *   length and therefore truncating keys and values to 4
 *   characters.
 *
 * * fixed two occurances of 'int out =' ... where the variable out
 *   is not used by hiding 'int out =' in a comment.
 *
 * * fixed a bad 'ungetc' call where the wrong variable was passed,
 *   's' instead of 'src' in 'parse_skipwhile'.
 *
 * the last two changes are marked with 'txb' in comments.
 */


/* forward declarations */

static
int
parse_expr(
	FILE *src,
	void *userdata,
	char *section,
	char *key,
	char *value,
	callback cb
);

static
int
parse_skipuntil(
	FILE *src,
	const char *s
);

static
int
parse_skipwhile(
	FILE *src,
	const char *s
);

static
int
parse_until(
	FILE *src,
	char *ptr,
	ssize_t maxlen,
	const char *s
);

static
int
parse_key(
	FILE *src,
	char *key
);

static
int
parse_value(
	FILE *src,
	char *value
);

static
int
stripright(
	char *c,
	const char *s
);

static
int
parse_kv(
	FILE *src,
	void *userdata,
	const char *section,
	char *key,
	char *value,
	callback cb
);

// the characters we consider to be whitespace
const char wss[] = " \t\r\n";
#define parse_skipws(src) parse_skipwhile(src, wss)

#define parse_section(src, section) parse_until(src, section, INI_SEC_MAXLEN, "]\n")

// if the callback returns non-zero, parsing will stop
// typedef int (*callback)(const char*, const char*, const char*, void*);

// we return the number of bytes handled, sort of, you'll see
int
parse_ini(
	FILE *src,
	void *userdata,
	callback cb
) {
	char section[INI_SEC_MAXLEN] = {0};
	char key[INI_KEY_MAXLEN] = {0};
	char value[INI_VAL_MAXLEN] = {0};
	int status, out = 0;
	// we stop going whenever we fail to consume any data, explicitly error,
	// the stream errors, or the stream ends
	while ((status = parse_expr(src, userdata, section, key, value, cb) >= 0)) {
		out += status;
		if (feof(src) || ferror(src)) break;
	}
	return ferror(src) ? -out : out;
}

static
int
parse_expr(
	FILE *src,
	void *userdata,
	char *section,
	char *key,
	char *value,
	callback cb
) {
	int len = parse_skipws(src);
	if (len) return len; // to avoid confusing byte counts, we're in a loop anyway

	int c;
	// figure out the next expression
	switch ((c = fgetc(src))) {
	case EOF:
		return 0; // let the outer loop figure out if this was an error or not
	case '[': // a section
		return parse_section(src, section);
	case '#':
	case ';':
		return parse_skipuntil(src, "\n");
	default: // key-value pair
		ungetc(c, src); // we need to conserve this one
		return parse_kv(src, userdata, section, key, value, cb);
	}
}

static
int
parse_skipuntil(
	FILE *src,
	const char *s
) {
	int out = 0;
	for (int c; (c = fgetc(src)) != EOF; out++) {
		if (strchr(s, c)) return out;
	}
	return ferror(src) ? -out : out;
}

static
int
parse_skipwhile(
	FILE *src,
	const char *s
) {
	int out = 0;
	for (int c; (c = fgetc(src)) != EOF; out++) {
		if (!strchr(s, c)) {
			ungetc(c, src); /* txb was ungetc(c, s) */
			return out;
		}
	}
	return ferror(src) ? -out : out;
}

static
int
parse_until(
	FILE *src,
	char *ptr,
	ssize_t maxlen,
	const char *s
) {
	int out = 0, c;
	while (out < maxlen) {
		c = fgetc(src);
		if (*ptr == EOF) { // hit error while scanning
			*ptr = 0;
			return ferror(src) ? -out : out;
		} else if (strchr(s, c)) {
			*ptr = 0;
			return out;
		}
		(*ptr++) = c; out++;
	}
	// we only make it here if we hit maxlen
	(*--ptr) = 0;
	int skipped = parse_skipwhile(src, s);
	if (skipped > 0) {
		return out + skipped; // errors are negative, eof is ok
	}
	return ferror(src) ? (skipped - out) : (out - skipped);
}

static
int
parse_key(
	FILE *src,
	char *key
) {
	/* txb int out = */
	parse_until(src, key, INI_KEY_MAXLEN, "=\n");
	return stripright(key, wss);
}

static
int
parse_value(
	FILE *src,
	char *value
) {
	/* txb int out = */
	parse_until(src, value, INI_VAL_MAXLEN, "\n");
	return stripright(value, wss);
}

// returns the number of output bytes
int
stripright(
	char *c,
	const char *s
) {
	ssize_t len = strlen(
			c); /* txb was strlen(s), which would truncate c to strlen(s) */
	if (!len) return len; // already empty
	while ((--len) >= 0 && strchr(s, c[len])) {}
	// either strchr failed or len is now -1
	if (len < 0) {
		*c = 0;
		return 0;
	}
	c[++len] = 0;
	return len;
}

int
parse_kv(
	FILE *src,
	void *userdata,
	const char *section,
	char *key,
	char *value,
	callback cb
) {
	int len = 0, tmp;

	tmp = parse_key(src, key); // consumes the =

	// if the key doesn't have a value or errored, we can't continue
	if (tmp <= 0 || feof(src)) return 0;
	len += tmp;

	tmp = parse_skipws(src); // the whitespace after the "="
	// if the value would have been empty, it ends up using the next line as the value
	// it may not error or eof, but it may be empty
	if (tmp < 0 || feof(src)) return 0;
	len += tmp;

	tmp = parse_value(src, value);
	// any errors are fine, since we've finished parsing now
	len += tmp > 0 ? tmp : -tmp;

	// let callback request terminating the parse by returning non-zero
	if (cb(section, key, value, userdata)) len *= -1;
	return len;
}

/* ckparser.c ends here */
