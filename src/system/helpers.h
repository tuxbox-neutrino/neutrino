
#ifndef __system_helpers__
#define __system_helpers__

/*
	Neutrino-HD

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
*/

int my_system(const char * cmd, const char * arg1, const char * arg2 = NULL, const char * arg3 = NULL, const char * arg4 = NULL, const char * arg5 = NULL, const char * arg6 = NULL);

int my_system(const char * cmd);

FILE* my_popen( pid_t& pid, const char *cmdstring, const char *type);
int safe_mkdir(char * path);
int check_dir(const char * newdir);
bool file_exists(const char *filename);
void wakeup_hdd(const char *hdd_dir);

#endif
