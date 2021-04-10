/*
	update-check

	Copyright (C) 2017 'vanhofen'
	Homepage: http://www.neutrino-images.de/

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

#ifndef __update_check__
#define __update_check__

class CFlashUpdateCheck
{
	public:
		CFlashUpdateCheck();
		~CFlashUpdateCheck();
		static CFlashUpdateCheck* getInstance();

		void	startThread();
		void	stopThread();

	private:

		pthread_t	c4u_thread;
		static void*	c4u_proc(void *arg);
};

#endif // __update_check__
