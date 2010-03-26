/********************************************************************
 * config_parser.c: Some cheesy recursive descent parser for the config file
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License. See http://creativecommons.org/licenses/by-sa/3.0/us/
 * for details.
 *
 * This file is part of thinkfan. See thinkfan.c for further info.
 * ******************************************************************/

#include "parser.h"
#include "globaldefs.h"
#include <stdlib.h>
#include <string.h>

const char space[] = " \t\n\r\f";
const char newline[] = "\n\r";
const char fan_keyword[] = "fan";
const char sensor_keyword[] = "sensor";
const char left_bracket[] = "({";
const char right_bracket[] = ")}";
const char comma[] = ",;";
const char digit[] = "-0123456789";
const char nonword[] = " \t\n\r\f,";
const char comment[] = "#";

// Not used atm...
const char snglquote[] = "\'";
const char dblquote[] = "\"";


/*
 * All these functions allocate memory only for the matching result, with the
 * exception of char_alt(), which never allocates memory and instead returns a
 * pointer to the last char that has been read from **input.
 */

/* Match any single char out of *items. Returns a pointer to the last read char
 * in **input. No memory is allocated, so returned chars should be copied
 * before using them. */
char *char_alt(char **input, const char *items, const char invert) {
	if (! **input) return NULL;
	while (*items)
		if (**input == *(items++)) return invert ? NULL : (*input)++;
	return invert ? (*input)++ : NULL;
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

/* Match an integer expression and return it as a int */
int *parse_int(char **input) {
	char *tmp, *invalid = "";
	int *rv = NULL;
	long int l;

	if (!(tmp = char_cat(input, digit, 0))) return NULL;
	rv = (int *) malloc(sizeof(int));
	l = strtol(tmp, &invalid, 0);
	if (*invalid != 0 || l > INT_MAX || l < INT_MIN) {
		free(rv);
		rv = NULL;
	}
	*rv = l;
	free(tmp);
	return rv;
}

/* Match a single string (keyword) */
char *parse_keyword(char **input, const char *keyword) {
	int l = strlen(keyword);
	char *ret;

	if (!strncasecmp(*input, keyword, l)) {
		ret = *input;
		*input += l;
		return ret;
	}
	return NULL;
}

void skip_space(char **input) {
	char *tmp = char_cat(input, space, 0);
	free(tmp);
}

char *parse_comment(char **input) {
	skip_space(input);
	if (!char_alt(input, comment, 0)) return NULL;
	char *tmp = char_cat(input, newline, 1);
	skip_space(input);
	return tmp;
}

char *parse_filename(char **input) {
	return char_cat(input, space, 1);
}


int parse_blankline(char **input) {
	skip_space(input);
	return **input == 0;
}

/* Return the string following a keyword. Matching ends at \n */
char *parse_statement(char **input, const char *keyword) {
	char *tmp, *ret = NULL;

	skip_space(input);
 	if (!(tmp = parse_keyword(input, keyword))) return NULL;
	skip_space(input);
	ret = parse_filename(input);
	return ret;
}

char *parse_fan(char **input) {
	char *start = *input;
	char *rv = parse_statement(input, fan_keyword);
	char *tmp = parse_comment(input);
	free(tmp);
	if (**input || !rv) {
		free(rv);
		*input = start;
		return NULL;
	}
	return rv;
}
char *skip_parse(char **input, const char *items, const char invert) {
	skip_space(input);
	return char_alt(input, items, invert);
}

/* Match an arbitrary-length tuple of int. Returns a NULL-terminated
 * array of pointers. */
int **parse_int_tuple(char **input) {
	int **rv = NULL, i = 0, j;
	int *tmp = NULL;

	if (!skip_parse(input, left_bracket, 0)) return NULL;
	do {
		skip_space(input);
		if (!(tmp = parse_int(input))) goto fail;
		rv = realloc(rv, sizeof(int *) * (i+2));
		rv[i++] = tmp;
	} while(skip_parse(input, comma, 0));
	rv[i] = NULL;
	if (!skip_parse(input, right_bracket, 0)) goto fail;
	return rv;

fail:
	for (j = 0; j < i; j++) free(rv[j]);
	free(rv);
	return NULL;
}

/* Parse a sensor statement followed by an optional bias tuple */
struct sensor *parse_sensor(char **input) {
	struct sensor *rv = (struct sensor *) malloc(sizeof(struct sensor));
	int **tmp, i;
	char *start = *input;

	if (!(rv->path = parse_statement(input, sensor_keyword))) {
		free(rv);
		*input = start;
		return NULL;
	}
	memset(rv->bias, 0, 16 * sizeof(int));
	skip_space(input);
	if ((tmp = parse_int_tuple(input)))
		for (i = 0; tmp[i] && i < 16; i++) {
			rv->bias[i] = *(tmp[i]);
			free(tmp[i]);
		}
	free(tmp);
	char *ignore = parse_comment(input);
	free(ignore);
	if (**input || !rv) {
		free(rv->path);
		free(rv);
		rv = NULL;
		*input = start;
	}
	return rv;
}

/* Match a tuple of the form ( int , int , int ) */
struct limit *parse_fan_level(char **input) {
	struct limit *rv = NULL;
	char *start = *input;
	int **tmp, i;

	if (!(tmp = parse_int_tuple(input))) goto fail;
	for (i = 0; i < 3; i++) if (!tmp[i]) goto fail;
	if (tmp[3]) goto fail;

	rv = malloc(sizeof(struct limit));
	rv->level = *(tmp[0]);
	rv->low = *(tmp[1]);
	rv->high = *(tmp[2]);
	skip_space(input);
	char *ignore = parse_comment(input);
	free(ignore);

fail:
	if (**input || !rv) {
		free(rv);
		rv = NULL;
		*input = start;
	}
	for (i=0; tmp && tmp[i]; i++) free(tmp[i]);
	free(tmp);
	return rv;
}

/*
char *parse_quotation(char **input, char *mark) {
	char *ret = NULL;
	char *start;
	if (!char_alt(input, mark, 0)) return NULL;
	start = *input;
	ret = char_cat(input, mark, 1);
	if (!char_alt(input, mark, 0)) return NULL;
	if (!ret) ret = "";
	return ret;
}//*/

