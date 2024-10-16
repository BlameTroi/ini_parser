/* ckparser.h */

#include <stdio.h>

#define INI_SEC_MAXLEN 64
#define INI_KEY_MAXLEN INI_SEC_MAXLEN
#define INI_VAL_MAXLEN INI_KEY_MAXLEN * 16

// if the callback returns non-zero, parsing will stop
typedef
int
(*callback)(
	const char *section, /* must be INI_SEC_MAXLEN + 1  */
	const char *key,     /*         INI_KEY_MAXLEN      */
	const char *value,   /*         INI_VAL_MAXLEN + 1  */
	void *user_data      /* any context for client code */
);

// we return the number of bytes handled, sort of, you'll see
int
parse_ini(
	FILE *src,
	void *userdata,
	callback cb
);

/* ckparser.h ends here */
