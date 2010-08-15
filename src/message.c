/********************************************************************
 * message.c
 *
 * This work is licensed under a Creative Commons Attribution-Share Alike 3.0
 * United States License. See http://creativecommons.org/licenses/by-sa/3.0/us/
 * for details.
 *
 * This file is part of thinkfan. See thinkfan.c for further info.
 * ******************************************************************/

#include "message.h"
#include "globaldefs.h"
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <syslog.h>

void report(int nlevel, int dlevel, char *format, ...) {
	va_list ap;
	int level = chk_sanity ? nlevel : dlevel;

	va_start(ap, format);
	if ( (nodaemon || isatty(fileno(stderr)))
			&& (!quiet || level > LOG_WARNING)) {
		fputs(prefix, stderr);
		vfprintf(stderr, format, ap);
		prefix = "";
	}
	else {
		vsyslog(level, format, ap);
	}
}
