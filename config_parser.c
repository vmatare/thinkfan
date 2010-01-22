/********************************************************************
 * config_parser.c: A simple recursive descent parser for the config file.
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License. See http://creativecommons.org/licenses/by-sa/3.0/us/
 * for details.
 *
 * This file is part of thinkfan. See thinkfan.c for further info.
 * ******************************************************************/

#include "config_parser.h"
#include "globaldefs.h"
#include <stdlib.h>
#include <string.h>

const char space[] = " \t\n\r\f";
const char newline[] = "\n";
const char fan_keyword[] = "fan";
const char sensor_keyword[] = "sensor";
const char left_bracket[] = "({";
const char right_bracket[] = ")}";
const char comma[] = ",;";
const char digit[] = "0123456789";

/*
 * All these functions allocate memory only for the matching string, with the
 * exception of char_alt(), which never allocates memory and instead returns a
 * pointer to the last char that has been read from **input.
 */

/* Match any single char out of *items. Returns a pointer to the last read char
 * in **input. No memory is allocated. */
char *char_alt(char **input, const char *items, const char invert) {
	while (*items) {
		if (invert ? **input != *items : **input == *items)
			return (*input)++;
		else items++;
	}
	return NULL;
}

/* Match an arbitrary sequence of any chars out of *items */
char *char_cat(char **input, const char *items, const char invert) {
	char *ret = NULL;
	char *start = *input;

	while (char_alt(input, items, invert));
	if (*input > start) {
		ret = (char*) calloc(*input - start + 2, sizeof(char));
		strncpy(ret, start, *input - start);
	}
	return ret;
}

char *parse_filename(char **input) {
	return char_cat(input, newline, 1);
}

void skip_space(char **input) {
	char *tmp = char_cat(input, space, 0);
	free(tmp);
}

/* Match an integer expression and return it as a long int */
long int *parse_long_int(char **input) {
	char *tmp, *invalid = "";
	long int *rv = NULL;


	skip_space(input);
	if (!(tmp = char_cat(input, digit, 0))) return NULL;
	rv = (long int *) malloc(sizeof(long int));
	*rv = (long int) strtol(tmp, &invalid, 0);
	if (*invalid != 0) {
		free(rv);
		rv = NULL;
	}
	free(tmp);
	return rv;
}

/* Match a single string (keyword) */
char *parse_keyword(char **input, const char *keyword) {
	int l = strlen(keyword);
	char *ret;

	skip_space(input);
	if (!strncasecmp(*input, keyword, l)) {
		ret = (char *) calloc(l + 1, sizeof(char));
		*input += l;
		return ret;
	}
	return NULL;
}

/* Return the string following a keyword. Matching ends at \n */
char *parse_statement(char **input, const char *keyword) {
	char *tmp, *ret = NULL;

	skip_space(input);
	if (!(tmp = parse_keyword(input, keyword))) return NULL;
	free(tmp);
	skip_space(input);
	ret = parse_filename(input);
	return ret;
}
char *parse_sensor(char **input) {
	return parse_statement(input, sensor_keyword);
}
char *parse_fan(char **input) {
	return parse_statement(input, fan_keyword);
}
char *skip_parse(char **input, const char *items, const char invert) {
	skip_space(input);
	return char_alt(input, items, invert);
}

/* Match a tuple of the form ( LEVEL , LOW , HIGH ) */
struct thm_tuple *parse_limit(char **input) {
	struct thm_tuple *rv = malloc(sizeof(struct thm_tuple));
	long int *i;
	if (!skip_parse(input, left_bracket, 0) || !(i = parse_long_int(input)))
		goto fail;
	rv->level = *i;
	free(i);
	if (!skip_parse(input, comma, 0) || !(i = parse_long_int(input)))
		goto fail;
	rv->low = *i;
	free(i);
	if (!skip_parse(input, comma, 0) || !(i = parse_long_int(input)))
		goto fail;
	rv->high = *i;
	free(i);
	if (!skip_parse(input, right_bracket, 0)) goto fail;
	return rv;
fail:
	free(rv);
	return NULL;
}

