/* iniparser.h -- an ini file parser based on one by Chloe Kudryavtsev */

#include <stdbool.h>
#include <stdio.h>

#define INI_SEC_MAXLEN 64
#define INI_KEY_MAXLEN INI_SEC_MAXLEN
#define INI_VAL_MAXLEN INI_KEY_MAXLEN * 16

/*
 * callback
 *
 * callback function for the parser. as each completed key:value
 * is parsed, post the data back to the parser client.
 *
 * in    : section, a string [section] from an ini file
 * in    : key, a string       key = value from an ini file
 * in    : value, a string
 * in/out: user_data, a pointer sized area that can be used
 *         to store or reference context for the parser client
 * return: a boolean, "should parser terminate?"
 */

typedef
bool
(*fn_callback)(
	const char *section, /* string length of INI_SEC_MAXLEN */
	const char *key,     /*                  INI_KEY_MAXLEN */
	const char *value,   /*                  INI_VAL_MAXLEN */
	void *user_data      /* any context for client code     */
);

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
 * in    : file stream on an ini file
 * in:   : expected to be a pointer, or any pointer sized
 *         object holding any state needed by the parser
 *         client.
 * in:   : function pointer of the callback function.
 * return: roughly the number of characters handled during
 *         parsing.
 */

int
parse_ini(
	FILE *ini_file,
	void *userdata,
	fn_callback callback
);

/* iniparser.h ends here */
