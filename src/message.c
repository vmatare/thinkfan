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

void report_tstat() {
	char *rv = NULL, *tmp;
	int i, len;

	rv = calloc(24, sizeof(char));
	strcpy(rv, "current temperatures: (");

	for (i=0; likely(i < num_temps); i++) {
		if (unlikely(i == triggered_tidx))
			len = asprintf(&tmp, "<%d>, ", temps[i]);
		else len = asprintf(&tmp, "%d, ", temps[i]);

		if (len < 1) {
			errcnt++;
			goto fail;
		}

		rv = realloc(rv, strlen(rv) + len + 1);
		strcat(rv, tmp);
		free(tmp);
	}
	rv[strlen(rv) - 2] = ')';
	rv[strlen(rv) - 1] = '\0';
	if (nodaemon) {
		puts(rv);
	}
	else {
		syslog(LOG_INFO, "%s", rv);
	}
fail:
	free(rv);
}
