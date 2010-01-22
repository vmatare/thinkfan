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

char *parse_sensor(char **input);
char *parse_fan(char **input);
struct thm_tuple *parse_limit(char **input);

void skip_space(char **input);

#endif
