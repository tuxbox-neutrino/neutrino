/** tzif-display.h                 http://libhdate.sourceforge.net
 *
 * Copyright:  2012 (c) Boruch Baum <zdump@gmx.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _TZIF_DISPLAY_H_
#define _TZIF_DISPLAY_H_

#include <stdio.h>		/// For printf, fopen, fclose, FILE
#include <stdlib.h>		/// For malloc, free
#include <error.h>		/// For error
#include <errno.h>		/// For errno
#include <string.h>		/// for memset, memcpy, memmem
#include <sys/stat.h>	/// for stat
#include <locale.h>		/// for setlocale
#include <time.h>       /// for ctime_r, tzset

/***************************************************
* definition of a tzfile header
***************************************************/
#define HEADER_LEN 44
typedef struct {
	char magicnumber[6];	  // = TZif2 or TZif\0
//	char reserved[15];		  // nulls
	long unsigned ttisgmtcnt; // number of UTC/local indicators stored in the file.
	long unsigned ttisstdcnt; // number of standard/wall indicators stored in the file.
	long unsigned leapcnt;    // number of leap seconds for which data is stored in the file.
	long unsigned timecnt;    // number of "transition times" for which data is stored in the file.
	long unsigned typecnt;    // number of "local time types" for which data is stored in the file (must not be zero).
	long unsigned charcnt;    // number of  characters  of  "timezone  abbreviation  strings" stored in the file.
	} timezonefileheader;
#define TZIF1_FIELD_SIZE 4
#define TZIF2_FIELD_SIZE 8

long parse_tz_long( const char *sourceptr, const int field_size);
int read_tz_header( timezonefileheader *header,  const char *temp_buffer);
char *tzif2_handle( timezonefileheader *tzh, const char *tzfile_buffer_ptr, size_t buffer_size );
char *display_tzfile_data(	const timezonefileheader tzh,
							const char* tzfile_buffer_ptr,
							const int field_size /// 4bytes for tzif1, 8bytes for tzif2
							);
const char *get_tzif_rule(const char *filename);

#endif
