/* testparser.c -- exercise the ini file parser */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iniparser.h"

/* the context is a pointer sized field that the callback function
 * can use for any purpose. */

void *bogus_ctx = NULL;

/* detect section breaks */

char last_section[INI_SEC_MAXLEN+1] = { 0 };

/*
 * callback function for the ini file parser. receives the current data.
 * for testing i'm just printing what we got and detecting breaks in
 * section.
 *
 * returning any vaue other than 0 tells the parser to quit.
 */

bool
cb_ini_parser(
	const char *section,
	const char *key,
	const char *value,
	void *ctx
) {
	if (strcmp(last_section, section) != 0)
		printf("\nsection    '%s'\n", section);
	strncpy(last_section, section, sizeof(last_section));
	printf("key:value  '%s':'%s'\n", key, value);
	return strcmp(section, "STOP") == 0
	&& strcmp(key, "STOP") == 0
	&& strcmp(value, "STOP") == 0;
}

/*
 * test driver.
 */

int
main(
	int argc,
	char **argv
) {
	printf("\n");
	if (argc < 2) {
		printf("error no file name given\n");
		return EXIT_FAILURE;
	}
	FILE *file = fopen(argv[1], "r");
	if (!file) {
		printf("error coult not open file %s\n", argv[1]);
		return EXIT_FAILURE;
	}
	last_section[0] = '\0';
	int parse_status = parse_ini(file, &bogus_ctx, cb_ini_parser);
	printf("\nparse complete, returned %d\n", parse_status);
	if (parse_status == EXIT_FAILURE) {
		printf("parse failed, check input file\n");
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

/* testparser.c ends here */
