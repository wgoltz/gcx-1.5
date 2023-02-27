/*******************************************************************************
  Copyright(c) 2000 - 2003 Radu Corlan. All rights reserved.
  
  This program is free software; you can redistribute it and/or modify it 
  under the terms of the GNU General Public License as published by the Free 
  Software Foundation; either version 2 of the License, or (at your option) 
  any later version.
  
  This program is distributed in the hope that it will be useful, but WITHOUT 
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
  more details.
  
  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 59 
  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
  
  Contact Information: radu@corlan.net
*******************************************************************************/

#define _GNU_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* add a little structure to error/log handling 
 * the latest error string (as printed by err_printf)
 * is retained in a static variable, so the calling 
 * function can find out what it was about.
 *
 * Of course, this is highly unthread-safe
 */

#define ERR_BUF_SIZE 1024
static char *lasterr_string = NULL;
int debug_level = 0;

int deb_printf(int level, const char *fmt, ...)
{
	va_list ap;
	int ret;

    if (level != debug_level)
		return 0;
	va_start(ap, fmt);
	ret = vfprintf(stderr, fmt, ap);
	va_end(ap);
	return ret;

}

/* print the error string and save it to storage */
int err_printf(const char *fmt, ...)
{
	va_list ap, ap2;
	int ret;
#ifdef __va_copy
	__va_copy(ap2, ap);
#else
	ap2 = ap;
#endif
	va_start(ap, fmt);
	va_start(ap2, fmt);
    if (lasterr_string) free(lasterr_string), lasterr_string = NULL;
    ret = vasprintf(&lasterr_string, fmt, ap2);
	if (ret > 0 && lasterr_string[ret-1] == '\n')
		lasterr_string[ret-1] = 0;
    ret = vfprintf(stderr, fmt, ap); fflush(NULL);

	va_end(ap);
	return ret;
}

/* clear the last error string (to make sure we don't get stale errors) */
void err_clear(void)
{
    if (lasterr_string) free(lasterr_string);
    lasterr_string = NULL;
}

/* get the error string */
char * last_err(void)
{
    return (lasterr_string) ? lasterr_string : "";
}

