/** tzif-display.c                 http://libhdate.sourceforge.net
 * display contents of tzif file   (in support of hcal and hdate)
 * hcal.c  Hebrew calendar              (part of package libhdate)
 * hdate.c Hebrew date/times information(part of package libhdate)
 *
 * compile:
 *     gcc -c -Wall -Werror -g tzif-display.c
 * build:
 *     gcc -Wall tzif-display.c -o tzif-display
 * run:
 *     ./tzif-display full-pathname-to-tzif-file
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

#include "tzif-display.h"

/***************************************************
* parse a tzfile "long format" value
***************************************************/
long parse_tz_long( const char *sourceptr, const int field_size)
{
	long retval = 0;
	char *long_ptr;

//	if (sizeof(long) < field_size)
//		printf("warning: field truncated because it is larger than a 'long' integer\n\n");

	int i,j;
	long_ptr = (char*) &retval;
	if ((field_size < (int)sizeof(long)) && (sourceptr[0] >> 8))
	{
		for (i=sizeof(long)-1; (i>=field_size); i--) long_ptr[i] = 255; 
	}
	j = 0;
	for (i=field_size-1; (i>=0) && (j<(int)sizeof(long)); i--)
	{
		long_ptr[j] = sourceptr[i];
		j++;
	}
	return retval;
}

/***************************************************
* read a tzheader file
***************************************************/
int read_tz_header( timezonefileheader *header,  const char *temp_buffer)
{
	const int field_size = 4;

	memcpy( header->magicnumber, &temp_buffer[0], 5 );
	header->magicnumber[5] = '\0';

	header->ttisgmtcnt = parse_tz_long(&temp_buffer[20], field_size);
	header->ttisstdcnt = parse_tz_long(&temp_buffer[24], field_size);
	header->leapcnt = parse_tz_long(&temp_buffer[28], field_size);
	header->timecnt = parse_tz_long(&temp_buffer[32], field_size);
	header->typecnt = parse_tz_long(&temp_buffer[36], field_size);
	header->charcnt = parse_tz_long(&temp_buffer[40], field_size);
#if 0
	printf("Header format ID: %s\n",header->magicnumber);
	printf("number of UTC/local indicators stored in the file = %ld\n", header->ttisgmtcnt);
	printf("number of standard/wall indicators stored in the file = %ld\n",header->ttisstdcnt);
	printf("number of leap seconds for which data is stored in the file = %ld\n",header->leapcnt);
	printf("number of \"transition times\" for which data is stored in the file = %ld\n",header->timecnt);
	printf("number of \"local time types\" for which data is stored in the file (must not be zero) = %ld\n",header->typecnt);
	printf("number of  characters  of  \"timezone  abbreviation  strings\" stored in the file = %ld\n\n",header->charcnt);
#endif
	if (header->typecnt == 0)
	{
		printf("Error in file format. Zero local time types suggested.\n");
		return false;
	}

	if (header->timecnt == 0)
	{
		printf("No transition times recorded in this file\n");
		return false;
	}

return true;
}

/***********************************************************************
* tzif2_handle
***********************************************************************/
char* tzif2_handle( timezonefileheader *tzh, const char *tzfile_buffer_ptr, size_t buffer_size )
{
	char *start_ptr;
	char magicnumber[6] = "TZif2";

	printf("tzif handle format 2\n");

	start_ptr = (char*)memmem( tzfile_buffer_ptr, buffer_size, &magicnumber, sizeof(magicnumber) );
	if (start_ptr == NULL)
	{
		printf("error finding tzif2 header\n");
		return NULL;
	}

	printf("tzif2 header found at position %d\n", (int)(start_ptr - tzfile_buffer_ptr));
	if ( read_tz_header( tzh, (char*) start_ptr ) == false )
	{
		printf("Error reading header file version 2\n");
		return NULL;
	}

	return start_ptr + HEADER_LEN;
}


/***************************************************
* Display contents of a timezone file
***************************************************/
char *display_tzfile_data(	const timezonefileheader tzh,
							const char* tzfile_buffer_ptr,
							const int field_size /// 4bytes for tzif1, 8bytes for tzif2
							)
{
	char *local_time_type_ptr;

	typedef struct {
		long gmtoff;	// seconds to add to UTC
		char is_dst;	// whether tm_isdst should be set by  localtime(3)
		char abbrind;	// index into the array of timezone abbreviation characters
	} ttinfo_struct;

	ttinfo_struct *ttinfo_data_ptr;
	char *ttinfo_read_ptr;
	char* tz_abbrev_ptr;
	char *leapinfo_ptr;
	char *wall_indicator_ptr;
	char *local_indicator_ptr;
	char *general_rule = NULL;

	local_time_type_ptr = (char*) tzfile_buffer_ptr + tzh.timecnt*field_size;
	ttinfo_read_ptr = local_time_type_ptr + tzh.timecnt;

	ttinfo_data_ptr = (ttinfo_struct*)malloc( sizeof(ttinfo_struct) * tzh.typecnt);
	if (ttinfo_data_ptr == NULL)
	{
		printf("memory allocation error - ttinfo_data\ntzname = ");
		return NULL;
	}

	/// timezone abbreviation list
	tz_abbrev_ptr = ttinfo_read_ptr + (tzh.typecnt * 6);

	leapinfo_ptr = tz_abbrev_ptr + (tzh.charcnt);

	wall_indicator_ptr = leapinfo_ptr + (tzh.leapcnt*field_size*2);

	local_indicator_ptr = wall_indicator_ptr + tzh.ttisstdcnt;

	if (field_size == TZIF2_FIELD_SIZE)
	{
		/// parse 'general rule' at end of file
		/// format should be per man(3) tzset
		char *start_line_ptr;
		start_line_ptr = strchr( local_indicator_ptr + tzh.ttisgmtcnt, 10 );
		if (start_line_ptr == NULL)
			printf("\nno \'general rule\' information found at end of file\n");
		else
		{
			char *end_line_ptr;
			end_line_ptr = strchr( start_line_ptr+1, 10 );
			if (end_line_ptr == NULL)
				printf("\nerror finding \'general rule\' info terminator symbol\n");
			else
			{
				*end_line_ptr = '\0';
				printf("general rule: %s\n", start_line_ptr+1 );
				general_rule = start_line_ptr+1;
			}
		}
	}
	free(ttinfo_data_ptr);
	return general_rule;
}

const char *get_tzif_rule(const char *filename)
{
	FILE *tz_file;
	char *tzif_buffer_ptr;
	char *start_ptr;
	int   field_size;
	const char *rule = NULL;
	struct stat file_status;
	timezonefileheader tzh;

	tz_file = fopen(filename, "rb");

	if (tz_file == NULL)
	{
		error(errno,0,"tz file %s not found\n", filename);
		return NULL;
	}

	if (fstat( fileno( tz_file), &file_status) != 0)
	{
		printf("error retreiving file status\n");
		fclose(tz_file);
		return NULL;
	}

	tzif_buffer_ptr = (char *) malloc( file_status.st_size );
	if (tzif_buffer_ptr == NULL)
	{
		printf("memory allocation error - tzif buffer\n");
		fclose(tz_file);
		return NULL;
	}

	if ( fread( tzif_buffer_ptr, file_status.st_size, 1, tz_file) != 1 )
	{
		printf("error reading into tzif buffer\n");
		free(tzif_buffer_ptr);
		return NULL;
	}
	fclose(tz_file);


	if ( read_tz_header( &tzh, tzif_buffer_ptr ) == false )
	{
		printf("Error reading header file version 1\n");
		free(tzif_buffer_ptr);
		return NULL;
	}

	if (tzh.magicnumber[4] == 50 )
	{
		start_ptr = tzif2_handle( &tzh, &tzif_buffer_ptr[HEADER_LEN], file_status.st_size - HEADER_LEN);
		field_size = TZIF2_FIELD_SIZE;
	}
	else
	{
		start_ptr = &tzif_buffer_ptr[HEADER_LEN];
		field_size = TZIF1_FIELD_SIZE;
	}
	if (start_ptr != NULL)
		rule = display_tzfile_data( tzh, start_ptr, field_size );

	free(tzif_buffer_ptr);
	return rule;
}
