/*
	(C)2014 by martii

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

#ifndef __LUASERVER_H__
#define __LUASERVER_H__
#include <global.h>
#include <semaphore.h>
#include <connection/basicserver.h>

class CLuaServer
{
	private:
		int count;
		pthread_t thr;

		static void *luaserver_thread(void *arg);
		static void *luaserver_main_thread(void *);
		static bool luaserver_parse_command(CBasicMessage::Header &rmsg, int connfd);
		static void Lock();
		static void UnLock();
		CLuaServer();
		~CLuaServer();
	public:
		sem_t may_run;
		bool did_run;

		void UnBlock();
		bool Block(const neutrino_msg_t msg, const neutrino_msg_data_t data);
		static CLuaServer *getInstance();
		static void destroyInstance();
};
#endif
