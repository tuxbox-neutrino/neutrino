/*
        md5sum lib  -   DBoxII-Project

        Copyright (C) 2001 Steffen Hehn 'McClean'
        Homepage: http://dbox.cyberphoria.org/



        License: GPL

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


	MD5-functions Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>.  
*/

#ifndef __libmd5sum__
#define __libmd5sum__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


int md5_file (const char *filename, int binary, unsigned char *md5_result);


#ifdef __cplusplus
}
#endif

#endif
