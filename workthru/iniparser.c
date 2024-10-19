/* iniparser.c -- an ini file parser based on one by Chloe Kudryavtsev */


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iniparser.h"

/*
 * this is inspired by source in a paper by chloe kudryavtsev _simply
 * parse in c_. the original is great for illustrating a parser, but
 * it won't plug directly in to my libraries.
 *
 * it's a good paper, you should read it.
 *
 * a minimum of c99 support is required. i compile with c18.
 *
 * notes:
 *
 * whitespace as used in this program does not include the newline \n.
 * newline is a hard break and closes whatever preceeds it on a
 * line.
 *
 * rules of ini:
 *
 * there is no standard definition, but ini files fall under "i know
 * it when i see it" rubric. this is a parser and it cares not one bit
 * about semantics.
 *
 * there are three possible expressions in an ini file definition:
 *
 * - comment line          # this is a comment ; so is this
 * - section header        [some text]
 * - key value pair        key = value
 *
 * blank lines are not significant. an expression may not span
 * multiple lines. leading and trailing whitespace are ignored.
 *
 * a null key in a key = value expression is an error "= something\n".
 *
 * a null value in a key = value epression is allowed "key =\n".
 *
 * the closing square brace on a section header may be omitted.
 */

/*
 * functions return one of the following status codes. remember that
 * the standard library says that if an error occurs on a file stream
 * eof will also be set. this means that ferror must be checked before
 * feof.
 */

#define STAT_ERROR  -1
#define STAT_EOF     0
#define STAT_OK      1

/*
 * skip_leading_whitespace
 *
 * skip past leading whitespace on a single line and leave the stream
 * positioned on the first non-whitespace character.
 *
 * in this program, a newline \n is a delimiter and not whitespace.
 *
 * in/out: file stream
 * return: integer STAT code
 *
 * remember that per the standard 'feof' is set for both a genuine eof
 * and for an error.
 */

static
int
skip_leading_whitespace(
	FILE* f
) {
	int c = fgetc(f);
	while (!feof(f) && (c == ' ' || c == '\r' || c == '\t'))
		c = fgetc(f); /* just churning */

	if (ferror(f))
		return STAT_ERROR;
	if (feof(f))
		return STAT_EOF;

	ungetc(c, f);
	return STAT_OK;
}

/*
 * trim_right
 *
 * remove trailing characters from a buffer. the characters to trim
 * are passed as a string for testing via 'strchr'.
 *
 * in/out: string buffer to trim
 * in    : string characters to remove
 * return: integer length of trimmed string
 *
 * the string 'buffer' is expected to be a properly termianted string.
 */

static
int
trim_right(
	char *buf,
	char *to_trim
) {
	/* start scan at byte before \0 terminator. if the string
	 * is empty, there's nothing more to do. */

	ssize_t len = strlen(buf);
	if (len < 1)
		return len;
	len -= 1;

	/* scan backwards for a character not in 'to_trim' or the
	 * start of the buffer is reached. */

	while (len >= 0 && strchr(to_trim, buf[len]) != NULL) {
		buf[len] = '\0';
		len -= 1;
	}

	/* the character after the current addressed by len
	 * is the new end of string. if len went negative it
	 * means that the string only contains characters
	 * found in 'to_trim'. */

	return len+1;
}

/*
 * flush_line
 *
 * nothing more is needed from the current line. the two most likely
 * reasons a line will be flushed are the line is a comment or there
 * are characters after a [section] declaration.
 *
 * in/out: file stream
 * return: STAT_OK or STAT_ERROR
 *
 * an end of file is considered 'ok' here. after the current line is
 * flushed the driver will start to read the next line and the eof
 * will be detected then.
 *
 * the stream is left positioned on the character following the \n.
 */

static
int
flush_line(FILE* f) {
	int c = '\0';

	while (c = fgetc(f), !feof(f) && c != '\n')
		;

	if (ferror(f))
		return STAT_ERROR;
	return STAT_OK;
}

/*
 * read_section
 *
 * read the section into a buffer.
 *
 * in/out: a file stream
 * in/out: string buffer large enough
 * in    : length of buffer
 * return: STAT_ERROR or STAT_OK or STAT_EOF
 *
 * the stream should be positioned on the character imediately
 * following the opening brace [. leading whitespace is ignored.
 * the remainder of the line after a closing brace ] is ignored.
 *
 * a missing closing brace ] is ignored.
 */

static
int
read_section(
	FILE *f,
	char *buffer,
	ssize_t buflen
) {
	int len = 0;
	int c = 0;
	bool skipping_ws = true;

	char *p = buffer;
	while (c = fgetc(f), !feof(f) && (c != '\n' && c != ']')) {
		if (skipping_ws && (c == ' ' || c == '\r' || c == '\t'))
			continue;
		skipping_ws = false;
		if (len == buflen)
			continue;
		len += 1;
		if (c == '\r' || c == '\t')
			c = ' ';
		*p = c;
		p += 1;
	}
	*p = '\0';

	if (ferror(f))
		return STAT_ERROR;
	if (feof(f))
		return STAT_EOF;

	if (c != '\n')
		return flush_line(f);

	return STAT_OK;
}

/*
 * read_key
 *
 * read the key expression ^\s*key\s*= into the buffer. any leading
 * whitespace characters are handled and the stream is expected to
 * be positioned on the first character of the key.
 *
 * if the line does not have an = after the key, it is an error.
 *
 * in/out: file stream
 * in/out: buffer large enough
 * in    : length of buffer
 * return: integer STAT code
 *
 */

static
int
read_key(
	FILE *f,
	char *buffer,
	ssize_t buflen
) {
	int len = 0;
	int c = 0;

	char *p = buffer;
	while (c = fgetc(f), !feof(f) && (c != '\n' && c != '=')) {
		if (len == buflen)
			continue;
		len += 1;
		if (c == '\r' || c == '\t')
			c = ' ';
		*p = c;
		p += 1;
	}
	*p = '\0';

	if (ferror(f))
		return STAT_ERROR;
	if (feof(f))
		return STAT_EOF;

	if (c != '=')
		return STAT_ERROR;

	trim_right(buffer, " \t\r");
	return strlen(buffer) > 0 ? STAT_OK : STAT_ERROR;
}

/*
 * read_value
 *
 * read the value expression value\s*\n where \n is the terminator.
 * the stream should be positioned on the first non whitespace
 * character following the 'key =' portion of the statement. trailing
 * blanks are removed.
 *
 * a line 'key = \n' will return an empty string as the value.
 *
 * in/out: file stream
 * in/out: buffer to hold the value, must be big enough to hold
 *         INI_VAL_MAXLEN characters
 * return: integer STAT code
 */

static
int
read_value(
	FILE *f,
	char *buffer,
	ssize_t buflen
) {
	int len = 0;
	int c = 0;

	char *p = buffer;
	while (c = fgetc(f), !feof(f) && c != '\n') {
		if (len == buflen)
			continue;
		len += 1;
		*p = c;
		p += 1;
	}
	*p = '\0';

	if (ferror(f))
		return STAT_ERROR;
	if (feof(f))
		return STAT_OK;
	/* eof is ok here. we ignore it and it will be detected on the
	 * next call to read_next. */

	trim_right(buffer, " \t\r");
	return STAT_OK;
}

/*
 * read_next_expression
 *
 * skip over intervening whitespace until an expression start is
 * recognized.
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
 * return: ~characters consumed
 *
 * as each value is collected, invoke the callback function
 * to give the clienet the current section, key, and value.
 */

static
int
read_next(
	FILE *f,
	char *section, ssize_t seclen,
	char *key, ssize_t keylen,
	char *value, ssize_t vallen
) {
	int iostat = STAT_OK;
	int c = '\0';

	do {
		iostat = skip_leading_whitespace(f);
		if (iostat != STAT_OK)
			return iostat;
		c = fgetc(f);
	} while (c == '\n');

	/* stream should now be positioned on the first non-whitespace
	 * character of the line to parse. expressions must each be on
	 * their own line.
	 *
	 * the type of the current expression is determined by its
	 * first character:
	 *
	 * # or ; - comment
	 *      [ - section header
	 *      = - this is an error, we are missing a key
	 *   else - start of a key = value pair */

	/* if a comment is read, flush to end of line and return.
	 * we'll worry about later lines when mainline calls us
	 * again. */

	if (c == '#' || c == ';') {
		iostat = flush_line(f);
		return iostat;
	}

	/* if a section header is read, the text up to the closing
	 * ] is trimmed and placed in the section field. then the
	 * line is flushed to the next \n. */

	if (c == '[') {
		memset(key, 0, keylen);
		memset(value, 0, vallen);
		iostat = read_section(f, section, seclen);
		return iostat;
	}

	/*
	 * from here we expect key = value on one line. any other configuration
	 * is reported as an error.
	 */

	/* no key found */
	if (c == '=') {
		return STAT_ERROR;
	}

	/* it looks like the start of key = value. put the first character
	 * of the key back on the stream for read_key. read_key reports an
	 * error if no = is found before \n. */

	ungetc(c, f);
	iostat = read_key(f, key, keylen);
	if (iostat != STAT_OK)
		return iostat;

	/* we had key =, now look for the first non-whitespace character,
	 * that's the start of the value. if we get a lone \n, we'll treat
	 * it as an empty string. */

	iostat = skip_leading_whitespace(f);
	if (iostat != STAT_OK)
		return iostat;

	c = fgetc(f);
	if (c == '\n')
		return STAT_OK;

	ungetc(c, f);
	return read_value(f, value, vallen);
}

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
 * sections may be empty, as in:
 *
 * [empty section]
 * # even though there's a blank line and comment line
 *
 * [another section]
 * extensions = .exe, .com, .dll
 * ...more key = value ...
 * [section immediately before eof is ok too]
 * --EOF--
 *
 * in/out: file stream on an ini file
 * in:   : expected to be a pointer, or any pointer sized
 *         object holding context for the client's callback
 *         function
 * in:   : function pointer of the callback function
 * return: roughly the number of characters handled during
 *         parsing
 *
 * if end of file is reached and no errors occured, EXIT_SUCCESS
 * is returned to the caller. on any error, EXIT_FAILURE is returned.
 *
 * the client's callback can request that the parse end early and that
 * will also return an EXIT_SUCCESS to the client.
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

	int iostat = 0;

	/* keep reading until one of the following occurs:
	 *
	 * - an error was raised
	 * - the stream ends
	 *
	 * pass current values to client callback.
	 * the client can request shutdown from their callback.
	 * otherwise loop back.
	 */

	do {
		iostat = read_next(ini_file,
				section, INI_SEC_MAXLEN,
				key, INI_KEY_MAXLEN,
				value, INI_VAL_MAXLEN);
		if (iostat != STAT_OK)
			break;

		/* if key is an empty string, we just read a
		 * section header. we don't invoke the callback
		 * until a key:value pair is read. */

		if (key[0] == '\0')
			continue;

		/*******************************
		 * post to client via callback *
		 *******************************/

		/* at this point we have a completed key:value pair,
		 * pass the section, key, and value back to the
		 * client.
		 *
		 * the client returns true if the parse should
		 * terminate early. */

		bool shutdown = callback(section, key, value, userdata);
		if (shutdown)
			break;
		memset(key, 0, INI_KEY_MAXLEN);
		memset(value, 0, INI_VAL_MAXLEN);

	} while (iostat == STAT_OK);

	if (iostat == STAT_ERROR)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

/* iniparser.c ends here */
