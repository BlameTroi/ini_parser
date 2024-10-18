/* iniparser.c -- an ini file parser based on one by Chloe Kudryavtsev */

/* feof is set when ferror is set. ferror is not set when feof is set.
 * the original code doesn't understand the distinction. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iniparser.h"

/*
 * the original source is from a paper by chloe kudryavtsev _simply
 * parse in c_. this is a rewrite of that code that follows my own
 * style. there's nothing wrong with the original style, it's just
 * not how i code.
 *
 * a minimum of c99 support is required. i compile with c18.
 *
 * bugs:
 *
 * - missing key or value throws parser out of sync from that point
 *   forward.
 */

/*
 * forward declarations, see the actual functions for documentation.
 */

static
int
read_next_expression(
	FILE *ini_file,
	char *section,
	char *key,
	char *value,
	void *userdata,
	fn_callback callback
);

static
int
skip_until(
	FILE *ini_file,
	const char *chars
);

/* static */
/* int */
/* skip_while( */
/*      FILE *ini_file, */
/*      const char *chars */
/* ); */

/* static */
/* int */
/* skip_whitespace( */
/*      FILE *ini_file */
/* ); */

static
int
read_section(
	FILE *ini_file,
	char *section
);

static
int
read_until(
	FILE *ini_file,
	char *ptr,
	ssize_t maxlen,
	const char *chars
);

static
int
read_key(
	FILE *ini_file,
	char *key
);

static
int
read_value(
	FILE *ini_file,
	char *value
);

static
int
trim_right(
	char *c,
	const char *chars
);

static
int
read_pair(
	FILE *ini_file,
	char *key,
	char *value
);

/*
 * we need to remember the last break character for error checking.
 */

static int last_break_char = 0;

/*
 * characters classes: whitespace and terminators.
 */

/* the usual whitespace characters */

/* static const char char_class_whitespace[] = " \t\r\n"; */

/* a section token ends with a closing brace or newline */

static const char char_class_section_end[] = "]\n";

/* a key token ends with an equal sign or newline. trailing
 * whitespace is trimmed off. */

/* static const char char_class_key_end[] = "=\n"; */

#define LINE_MAX 4096
static char *line = NULL;

/*
 * ini_parse
 *
 * given a properly opened and position file stream, parse it
 * as an ini file. an ini file is made up of comments (# or ;),
 * sections ([section name]), and key:value pairs (key = value).
 *
 * as each key:value pair is completed, post the section, key,
 * and value to a client provided callback.
 *
 * in/out: file stream on an ini file
 * in:   : expected to be a pointer, or any pointer sized
 *         object holding context for the client's callback
 *         function
 * in:   : function pointer of the callback function
 * return: roughly the number of characters handled during
 *         parsing
 *
 * a negative return means there was some error in the input
 * stream.
 */

int
parse_ini(
	FILE *ini_file,
	void *userdata,
	fn_callback callback
) {
	char section[INI_SEC_MAXLEN+1] = {0};
	char key[INI_KEY_MAXLEN+1] = {0};
	char value[INI_VAL_MAXLEN+1] = {0};

	int len_read = 0;
	int chars_out = 0;

	last_break_char = 0;

	/* keep reading until one of the following occurs:
	 *
	 * - an error was raised (negative length_read)
	 * - the stream ends
	 * - the stream errors
	 */

	line = malloc(LINE_MAX+1);
	memset(line, 0, LINE_MAX+1);

	while (
		len_read = read_next_expression(ini_file, section, key, value, userdata,
				callback),
		len_read >= 0
	) {
		if (len_read == 0 && feof(ini_file) && !ferror(ini_file)) {
			free(line);
			return chars_out;
		}
		chars_out += len_read;
	}

	/* we only get here if there was an error (negative) return
	 * from read_next_expression. negate our result and send it
	 * to the client. */

	free(line);
	return -chars_out;
}

/*
 * read_next_expression
 *
 * skip over intervening whitespace until an expression
 * start is recognized.
 *
 * the expressions:
 *
 * [section name]
 * ; comment
 * # another comment
 * key = value
 *
 * in/out: a file stream
 * in/out: string for section, expected to be INI_SEC_MAXLEN long
 * in/out: string for key, expected to be INI_KEY_MAXLEN long
 * in/out: string for value, expected to be INI_VAL_MAXLEN long
 * in    : the client's optional context
 * in    : client callback function
 * return: ~characters consumed
 *
 * as each value is collected, invoke the callback function
 * to give the clienet the current section, key, and value.
 */
#define IOSTAT_ERROR  -1
#define IOSTAT_OK      0
#define IOSTAT_EOF     1

/*
 * skip leading whitespace on a line, passing over multiple lines
 * if multiple blank lines are encountered.
 *
 * note that per spec feof is set for both a genuine eof and for
 * an error.
 */

static
int
skip_leading_whitespace(FILE* f) {
	int c = 0;
	while (c = fgetc(f), !feof(f) && (c == ' ' || c == '\r' || c == '\t' ||
	c == '\n'))
		; /* just churning */
	if (ferror(f))
		return IOSTAT_ERROR;
	if (feof(f))
		return IOSTAT_EOF;
	ungetc(c, f);
	return IOSTAT_OK;
}

/*
 * we are done with this line, advance to the next newline and
 * leave the file positioned at the next character. end of file
 * is considered next newline for our purposes.
 */

static
int
flush_line(FILE* f) {
	int c = 0;
	while (c = fgetc(f), !feof(f) && c != '\n')
		;
	if (ferror(f))
		return IOSTAT_ERROR;
	return IOSTAT_OK;
}

static
int
read_next_expression(
	FILE *ini_file,
	char *section,
	char *key,
	char *value,
	void *userdata,
	fn_callback callback
) {

	int iostat = skip_leading_whitespace(ini_file);
	if (iostat == IOSTAT_ERROR)
		return -1;
	if (iostat == IOSTAT_EOF)
		return 0;

	/* stream should now be positioned on the first non-whitespace
	 * character of the line to parse. expressions must each be on
	 * their own line.
	 *
	 *
	 * the type of the current expression is determined by its
	 * first character:
	 *
	 * # or ; - comment
	 *      [ - section header
	 *      = - this is an error, we are missing a key
	 *   else - start of a key = value pair
	 *
	 */

	int c = fgetc(ini_file);

	switch (c) {

	case EOF:         /* eof or error are reported here as eof. */
		if (ferror(ini_file))
			return -1;
		return 0;

	case '[':         /* start of section, [text(]|\n). */
		return read_section(ini_file, section);

	case '#':         /* start of a comment, either shell */
	case ';':         /* or assembly langauge style. */
		iostat = flush_line(ini_file);
		if (ferror(ini_file))
			return -1;
		return 1;

	case '=':         /* missing key */
		ungetc(c, ini_file);
		return -1;

	default:          /* assumed to be the start of a key = value pair */
		/* put the character back so we can collect it as the first
		 * character of the key. */
		ungetc(c, ini_file);
	}
	int len = read_pair(ini_file, key, value);

	/*******************************
	 * post to client via callback *
	 *******************************/

	/* client returns true if the parse should terminate
	 * early. */
	if (callback(section, key, value, userdata))
		len *= -1;

	return len;
}

/*
 * skip_until
 *
 * consume characters until one of *chars is matched.
 *
 * in/out: a file stream
 * in    : a string of character terminators
 * result: number of characters consumed
 *
 * errors are reported by negating the count of consumed
 * characters.
 */

static
int
skip_until(
	FILE *ini_file,
	const char *chars
) {
	int consumed = 0;
	int c;

	while (c = fgetc(ini_file), c != EOF) {
		if (strchr(chars, c)) {
			last_break_char = c;
			return consumed;
		}
		consumed += 1;
	}

	/* we reached eof or there was an error while
	 * reading. if there was an error, report the
	 * count as a negative value. */

	last_break_char = EOF;
	return ferror(ini_file) ? -consumed : consumed;
}

/*
 * skip_while
 *
 * consume characters until one of *chars is matched.
 * put that character back into the stream as it is part
 * of the next token.
 *
 * in/out: a file stream
 * in    : a string of characters to skip
 * result: ~number of characters skipped
 *
 * errors are reported by negating the count of skipped
 * characters.
 */

/* static */
/* int */
/* skip_while( */
/*      FILE *ini_file, */
/*      const char *chars */
/* ) { */
/*      int skipped = 0; */
/*      int c; */

/*      while (c = fgetc(ini_file), c != EOF) { */
/*              if (strchr(chars, c) == NULL) { */
/*                      last_break_char = c; */
/*                      ungetc(c, ini_file); */
/*                      return skipped; */
/*              } */
/*              skipped += 1; */
/*      } */

/*      /\* we reached eof or there was an error while */
/*       * reading. if there was an error, report the */
/*       * count as a negative value. *\/ */

/*      last_break_char = EOF; */
/*      return ferror(ini_file) ? -skipped : skipped; */
/* } */

/*
 * skip_whitespace
 *
 * in/out: a file stream
 * result: ~number of characters skipped
 *
 * errors are reported by negating the count of skipped
 * characters.
 */

/* static */
/* int */
/* skip_whitespace( */
/*      FILE *ini_file */
/* ) { */
/*      return skip_while(ini_file, char_class_whitespace); */
/* } */

/* static */
/* int */
/* skip_same_line_whitespace( */
/*      FILE *ini_file */
/* ) { */
/*      return skip_while(ini_file, " \t\r"); */
/* } */
/*
 * read_section
 *
 * read the section into the section buffer.
 *
 * in/out: a file stream
 * in/out: string big enough to hold INI_SEC_MAXLEN characters.
 * return: ~characters consumed.
 *
 * any errors in read_until are not detected here but are
 * passed back unchanged.
 */

static
int
read_section(
	FILE *ini_file,
	char *section
) {
	return read_until(ini_file, section, INI_SEC_MAXLEN, char_class_section_end);
}

/*
 * read_until
 *
 * append characters until one matching in *chars is read
 * or maxlen is reached.
 *
 * in/out: a file stream
 * in/out: string big enough to hold the max characters
 *         allowed
 * in    : max characters allowed in buffer
 * in    : string of terminators for the current expression
 * return: ~characters consumed
 *
 * a negative return value means some error occured.
 */

static
int
read_until(
	FILE *ini_file,
	char *buffer,
	ssize_t buffer_max,
	const char *terminators
) {
	int appended = 0;
	int c;

	/* read until error, eof, or output buffer length
	 * exceeded. */

	while (appended < buffer_max) {
		c = fgetc(ini_file);

		/* terminal conditions are eof, io error, or
		 * reading a terminal character. */

		if (c == EOF) {
			last_break_char = EOF;
			*buffer = '\0';
			return ferror(ini_file) ? -appended : appended;
		} else if (strchr(terminators, c) != NULL) {
			last_break_char = c;
			*buffer = '\0';
			return appended;
		}

		/* append and advance */

		*buffer = c;
		buffer += 1;
		appended += 1;
	}

	/* end of output buffer reached before eof or a
	 * terminal character. place a NUL for end of string
	 * into the buffer and then skip over characters
	 * until eof or a terminal character is read. */

	buffer -= 1;
	*buffer = '\0';

	int skipped = skip_until(ini_file, terminators);
	if (skipped > 0)
		return appended + skipped;

	last_break_char = EOF;
	return ferror(ini_file) ? (skipped - appended) : (appended - skipped);
}

/*
 * read_key
 *
 * read the key expression ^\s*key\s*= into the buffer. the leading
 * blanks are handled elsewhere, but trailing blanks are trimmed here.
 *
 * in/out: file stream
 * in/out: buffer to hold the key, must be big enough to hold
 *         INI_KEY_MAXLEN characters
 * return: ~characters consumed
 *
 * a negative return value means some error occured.
 */

static
int
read_key(
	FILE *f,
	char *buffer
) {
	int ret = 0;
	int c = 0;
	while (c = fgetc(f), !feof(f) && (c != '\n' && c != '=')) {
		if (ret == INI_KEY_MAXLEN)
			continue;
		ret += 1;
		if (c == '\r' || c == '\t')
			c = ' ';
		*buffer = c;
		buffer += 1;
	}
	*buffer = '\0';
	if (ferror(f))
		return -ret;
	if (feof(f))
		return ret;
	ungetc(c, f);
	return ret;
}

/*
 * read_value
 *
 * read the value expression value\s*\n where \n is the terminator.
 * leading blanks are handled elsewhere, but trailing blanks are
 * trimmed here.
 *
 * in/out: file stream
 * in/out: buffer to hold the value, must be big enough to hold
 *         INI_VAL_MAXLEN characters
 * return: ~characters consumed
 *
 * a negative return value means some error occured.
 */

static
int
read_value(
	FILE *f,
	char *buffer
) {
	/* int out = read_until(ini_file, value, INI_VAL_MAXLEN, "\n"); */
	/* out += trim_right(value, char_class_whitespace); */
	/* return out; */

	int ret = 0;
	int c = 0;
	while (c = fgetc(f), !feof(f) && c != '\n') {
		if (ret == INI_VAL_MAXLEN)
			continue;
		ret += 1;
		*buffer = c;
		buffer += 1;
	}
	*buffer = '\0';
	if (ferror(f))
		return -ret;
	/* no need to push the last character back */
	return ret;

}

/*
 * trim_right
 *
 * remove trailing characters from the buffer.
 *
 * in/out: current position in buffer
 * in    : characters to remove
 * return: ~characters left in buffer
 */

int
trim_right(
	char *buf,
	const char *to_trim
) {
	ssize_t len = strlen(buf);
	if (len < 1)
		return len;

	/* scan backwards for a character not in to_trim. */
	len -= 1;
	while (len >= 0 && strchr(to_trim, buf[len]) != NULL)
		len -= 1;

	/* either failed to find a trim character or length
	 * has gone negative. mark end of string and return
	 * a valid length. */

	if (len < 0) {
		*buf = 0;
		return 0;
	}
	buf[len + 1] ='\0';
	return len + 1;
}

/*
 * read_pair
 *
 * assuming start of key, collect key, skip =, then value.
 *
 * in/out: file stream
 * in/out: buffer to hold the key, must be big enough to hold
 *         INI_KEY_MAXLEN characters
 * in/out: buffer to hold the value, must be big enough to
 *         hold INI_VAL_MAXLEN characters.
 * return: ~characters consumed
 *
 * bug: either here or in read_next_expression we need to
 *      recognize a missing key (first candidate character
 *      is '=').
 */

int
read_pair(
	FILE *ini_file,
	char *key,
	char *value
) {
	int len = 0;

	/* read the key, advance past to either = or \n. */

	len = read_key(ini_file, key);

	/* if the key doesn't have a value (eof or some error encountered)
	 * we can't continue. */

	if (len <= 0 || feof(ini_file))
		return 0;

	len -= trim_right(key, " \r\t");

	if (strlen(key) == 0)
		return -1;

	/* check break character, if it isn't an =, we have an error */
	int c = fgetc(ini_file);
	if (c != '=')
		return -len;

	if (skip_leading_whitespace(ini_file) != IOSTAT_OK)
		return -len;

	/* get the breaking character, if it's a newline, it's an error */
	c = fgetc(ini_file);
	if (c == '\n')
		return -len;
	ungetc(c, ini_file);

	int tmp = read_value(ini_file, value);
	if (tmp <= 0 || ferror(ini_file)) /* not feof here, eof is legal */
		return -len;

	tmp = trim_right(key, " \r\t");

	if (strlen(key) == 0)
		return -len;

	len += tmp;

	return len;
}

/* iniparser.c ends here */
