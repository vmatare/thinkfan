/********************************************************************
 * config_parser.h: Header for the config parser
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License. See http://creativecommons.org/licenses/by-sa/3.0/us/
 * for details.
 *
 * This file is part of thinkfan. See thinkfan.c for further info.
 * ******************************************************************/

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

struct sensor *parse_sensor(char **input);
char *parse_fan(char **input);
struct limit *parse_fan_level(char **input);
char *parse_keyword(char **input, const char *keyword);
char *parse_comment(char **input);
int parse_blankline(char **input);

void skip_space(char **input);
int *parse_int(char **input);

#endif
