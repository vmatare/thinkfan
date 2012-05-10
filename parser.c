/********************************************************************
 * config_parser.c: Some cheesy recursive descent parser for the config file
 *
 * this file is part of thinkfan. See thinkfan.c for further information.
 *
 * thinkfan is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * thinkfan is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with thinkfan.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ******************************************************************/

#include "parser.h"
#include "globaldefs.h"
#include "system.h"

#ifdef USE_ATASMART
static const char atasmart_keyword[] = "atasmart";
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include "message.h"

static const char space[] = " \t";
static const char newline[] = "\n\r\f";
static const char fan_keyword[] = "fan";
static const char sensor_keyword[] = "sensor";
static const char hwmon_keyword[] = "hwmon";
static const char tp_thermal_keyword[] = "tp_thermal";
static const char tp_fan[] = "tp_fan";
static const char pwm_fan[] = "pwm_fan";
static const char left_bracket[] = "({";
static const char right_bracket[] = ")}";
static const char separator[] = ",; ";
static const char nonword[] = " \t\n\r\f,;#({})";
static const char comment[] = "#";
//static const char nonfilename[] = "\n\n{[";
static const char digit[] = "0123456789";
static const char quote[] = "\"";
static const char period[] = ".";


/*
 * All these functions allocate memory only for the matching result, with the
 * exception of char_alt(), which never allocates memory and instead returns a
 * pointer to the last char that has been read from **input.
 * All advance the **input pointer to point to the first char that has not been
 * parsed. If parsing was unsuccessful, **input is reset to where it started.
 */

/* Match any single char out of *items. Returns a pointer to the last read char
 * in **input. No memory is allocated, so returned chars should be copied
 * before using them. */
char *char_alt(char **input, const char *items, const char invert) {
	if (! **input) return NULL;
	while (*items)
		if (**input == *(items++)) {
			if (invert) return NULL;
			else {
				if (**input == '\n') line_count++;
				return (*input)++;
			}
		}
	if (invert) {
		if (**input == '\n') line_count++;
		return (*input)++;
	}
	return NULL;
}

/* Match an arbitrary sequence of any chars out of *items */
char *char_cat(char **input, const char *items, const char invert) {
	char *ret = NULL;
	char *start = *input;
	int oldlc = line_count;

	while (char_alt(input, items, invert));
	if (*input > start) {
		ret = (char*) calloc(*input - start + 2, sizeof(char));
		strncpy(ret, start, *input - start);
	}
	else line_count = oldlc;
	return ret;
}

/* Match an integer expression and return it as an int */
int *parse_int(char **input) {
	char *end = *input;
	int *rv = NULL;
	long int l;

	l = strtol(*input, &end, 0);
	if (end > *input && l <= INT_MAX && l >= INT_MIN) {
		*input = end;
		rv = (int *) calloc(2, sizeof(int));
		*rv = (int) l;
		rv[1] = INT_MIN;
	}
	skip_comment(input);
	return rv;
}

/* Match a single string (keyword) */
char *parse_keyword(char **input, const char *keyword) {
	char *start = *input;
	int l = strlen(keyword);
	char *ret = NULL;

	if (!strncasecmp(*input, keyword, l)) {
		ret = *input;
		*input += l;
		skip_comment(input);
	}
	else *input = start;
	return ret;
}

void skip_space(char **input) {
	char *tmp = char_cat(input, space, 0);
	free(tmp);
}

char *parse_comment(char **input) {
	skip_space(input);
	if (!char_alt(input, comment, 0)) return NULL;
	char *tmp = char_cat(input, newline, 1);
	char_alt(input, newline, 0);
	if (tmp == NULL) {
		tmp = malloc(sizeof(char));
		*tmp = 0;
	}
	return tmp;
}

void skip_comment(char **input) {
	char *tmp = parse_comment(input);
	free(tmp);
}

char *parse_word(char **input) {
	return char_cat(input, nonword, 1);
}

/*char *parse_newline(char **input) {
	char *rv;
	if (char_alt(input, newline, 0)) {
		rv = malloc(sizeof(char));
		*rv = **input;
		return rv;
	}
	return NULL;
}*/

char *parse_blankline(char **input) {
	skip_space(input);
	return char_alt(input, newline, 0);
}

void skip_blankline(char **input) {
	parse_blankline(input);
}

void skip_line(char **input) {
	char *tmp = char_cat(input, newline, 1);
	skip_blankline(input);
	free(tmp);
}

/* Return the string/quotation following a keyword. */
char *parse_statement(char **input, const char *keyword) {
	char *start = *input;
	char *tmp, *ret = NULL;
	int oldlc = line_count;

	skip_space(input);
	if (!(tmp = parse_keyword(input, keyword))) goto done;
	skip_space(input);
	if (!(ret = parse_quotation(input, quote)))
		ret = parse_word(input);
	skip_comment(input);
done:
	if (!ret) {
		line_count = oldlc;
		*input = start;
	}
	return ret;
}

char *parse_fan(char **input) {
	return parse_statement(input, fan_keyword);
}

char *parse_tpfan(char **input) {
	return parse_statement(input, tp_fan);
}

char *parse_pwmfan(char **input) {
	return parse_statement(input, pwm_fan);
}

char *skip_parse(char **input, const char *items, const char invert) {
	skip_space(input);
	return char_alt(input, items, invert);
}

/* Match an arbitrary-length tuple of int. Returns an array of int,
 * terminated by INT_MIN. */
int *parse_int_tuple(char **input) {
	int *rv = NULL, i = 0;
	int *tmp = NULL;
	int oldlc = line_count;

	if (!skip_parse(input, left_bracket, 0)) goto fail;
	skip_comment(input);
	do {
		if (!(tmp = parse_int(input))) {
			if (skip_parse(input, period, 0)) {
				tmp = malloc(sizeof(int));
				*tmp = INT_MAX;
			}
			else goto fail;
		}
		rv = realloc(rv, sizeof(int) * (i+2));
		rv[i++] = *tmp;
		free(tmp);
		skip_comment(input);
		skip_parse(input, separator, 0);
	} while(!skip_parse(input, right_bracket, 0));
	rv[i] = INT_MIN;
	skip_comment(input);
	return rv;

fail:
	line_count = oldlc;
	free(rv);
	return NULL;
}

struct limit *parse_level(char **input) {
	struct limit *rv = NULL;
	char *start = *input;
	int oldlc = line_count;

	if (!skip_parse(input, left_bracket, 0)) goto fail3;
	skip_space(input);
	skip_comment(input);
	skip_blankline(input);

	rv = (struct limit *) malloc (sizeof(struct limit));

	// OK, fan levels are strings now.
	if ( !((rv->level = char_cat(input, digit, 0))
			|| (rv->level = parse_quotation(input, quote))) )
		goto fail3;

	skip_parse(input, separator, 0);
	skip_comment(input);
	skip_blankline(input);

	if (!(rv->low = parse_int_tuple(input))
			&& !(rv->low = parse_int(input))) goto fail2;

	skip_parse(input, separator, 0);
	skip_comment(input);
	skip_blankline(input);

	if(!(rv->high = parse_int_tuple(input))
			&& !(rv->high = parse_int(input))) goto fail1;

	skip_comment(input);
	skip_blankline(input);
	if (!skip_parse(input, right_bracket, 0)) goto fail;
	skip_space(input);
	return rv;

fail:
	free(rv->high);
fail1:
	free(rv->low);
fail2:
	free(rv->level);
fail3:
	free(rv);
	line_count = oldlc;
	*input = start;
	return NULL;
}

/* Parse a sensor statement followed by an optional bias tuple */
static struct sensor *parse_tempinput(char **input, const char *keyword) {
	struct sensor *rv = (struct sensor *) malloc(sizeof(struct sensor));
	int *tmp, i, oldlc = line_count;
	char *start = *input;

	if (!(rv->path = parse_statement(input, keyword))) {
		free(rv);
		*input = start;
		line_count = oldlc;
		return NULL;
	}
	memset(rv->bias, 0, 16 * sizeof(int));
	skip_space(input);
	if ((tmp = parse_int_tuple(input)))
		for (i = 0; (tmp[i] != INT_MIN) && i < 16; i++) {
			rv->bias[i] = tmp[i];
		}
	free(tmp);
	skip_comment(input);
	skip_space(input);
	return rv;
}

struct sensor *parse_sensor(char **input) {
	struct sensor *rv;
	if ((rv = parse_tempinput(input, sensor_keyword))) {
		report(LOG_WARNING, LOG_WARNING, MSG_WRN_SENSOR_DEPRECATED);
		if (!strcmp(rv->path, IBM_TEMP)) rv->get_temp = get_temp_ibm;
		else rv->get_temp = get_temp_sysfs;
	}
	else if ((rv = parse_tempinput(input, tp_thermal_keyword)))
		rv->get_temp = get_temp_ibm;
	else if ((rv = parse_tempinput(input, hwmon_keyword)))
		rv->get_temp = get_temp_sysfs;
#ifdef USE_ATASMART
	else if ((rv = parse_tempinput(input, atasmart_keyword)))
		rv->get_temp = get_temp_atasmart;
#endif
	return rv;
}

char *parse_quotation(char **input, const char *mark) {
	char *ret = NULL;
	char *start;
	int oldlc = line_count;

	start = *input;
	if (!char_alt(input, mark, 0)) return NULL;
	ret = char_cat(input, mark, 1);
	if (!ret) {
		ret = malloc(sizeof(char));
		*ret = 0;
	}
	if (!char_alt(input, mark, 0)) {
		free(ret);
		ret = NULL;
		line_count = oldlc;
	}
	return ret;
}

