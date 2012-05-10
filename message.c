/********************************************************************
 * message.c
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

#include "message.h"
#include "globaldefs.h"
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

void report(int nlevel, int dlevel, char *format, ...) {
	va_list ap;
	int level = chk_sanity ? nlevel : dlevel;

	va_start(ap, format);
	if (nodaemon && (!quiet || level > LOG_WARNING)) {
		fputs(prefix, stderr);
		vfprintf(stderr, format, ap);
		prefix = "";
	}
	else {
		vsyslog(level, format, ap);
	}
}
