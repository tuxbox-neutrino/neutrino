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

#if 0
For testing try:

cat <<EOT > /share/tuxbox/neutrino/luaplugins/test.lua
#!/bin/luaclient

for i,v in ipairs(arg) do
	print(tostring(i) .. "\t" .. tostring(v))
end
return "ok"
EOT

chmod +x /share/tuxbox/neutrino/luaplugins/test.lua

/share/tuxbox/neutrino/luaplugins/test.lua a b c d
#endif

#include <config.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <poll.h>

#include <global.h>
#include <neutrino.h>
#include <system/helpers.h>
#include <system/set_threadname.h>
#include <luaclient/luaclient.h>
#include <gui/lua/luainstance.h>

#include "luaserver.h"

static CLuaServer *instance = NULL;
static pthread_mutex_t mutex;

CLuaServer *CLuaServer::getInstance()
{
	if (!instance)
		instance = new CLuaServer();
	return instance;
}

void CLuaServer::destroyInstance()
{
	Lock();
	if (instance) {
		delete instance;
		instance = NULL;
	}
	UnLock();
}

CLuaServer::CLuaServer()
{
	count = 0;
	did_run = false;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&mutex, &attr);
	pthread_mutexattr_destroy(&attr);

	sem_init(&may_run, 0, 0);

	pthread_create (&thr, NULL, luaserver_main_thread, (void *) NULL);
}

CLuaServer::~CLuaServer()
{
	pthread_cancel(thr);
	pthread_join(thr, NULL);
	sem_destroy(&may_run);
	pthread_mutex_destroy(&mutex);
	instance = NULL;
}

void CLuaServer::UnBlock()
{
	sem_post(&may_run);
}

bool CLuaServer::Block(const neutrino_msg_t msg, const neutrino_msg_data_t data)
{
	sem_wait(&may_run);
	if (did_run) {
		if (msg != CRCInput::RC_timeout)
			g_RCInput->postMsg(msg, data);
		did_run = false;
		return true;
	}
	return false;
}

class luaserver_data
{
	public:
		CLuaInstance *lua;
		bool died;
		int fd;

		std::vector<std::string> argv;
		std::string script;

		luaserver_data(int _fd, std::string &_script) {
			fd = dup(_fd);
			fcntl(fd, F_SETFD, FD_CLOEXEC);
			script = _script;
			died = false;
		}
		~luaserver_data(void) {
			close(fd);
		}
};

void CLuaServer::Lock(void)
{
	pthread_mutex_lock(&mutex);
}

void CLuaServer::UnLock(void)
{
	pthread_mutex_unlock(&mutex);
}

void *CLuaServer::luaclient_watchdog(void *arg) {
	set_threadname(__func__);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	luaserver_data *lsd = (class luaserver_data *)arg;

	struct pollfd pfd;
	pfd.fd = lsd->fd;
	pfd.events = pfd.revents = 0;
	poll(&pfd, 1, -1);
	lsd->lua->abortScript();
	lsd->died = true;
	pthread_exit(NULL);
}

void *CLuaServer::luaserver_thread(void *arg) {
	set_threadname(__func__);
	Lock();
	if (instance) {
		instance->count++;
		instance->did_run = true;
	}
	UnLock();

	luaserver_data *lsd = (class luaserver_data *)arg;

	pthread_t wdthr;
	pthread_create (&wdthr, NULL, luaclient_watchdog, (void *) lsd);

	CLuaInstance lua;
	lsd->lua = &lua;
	std::string result_code;
	std::string result_string;
	std::string error_string;
	lua.runScript(lsd->script.c_str(), &lsd->argv, &result_code, &result_string, &error_string);
	pthread_cancel(wdthr);
	pthread_join(wdthr, NULL);
	if (!lsd->died) {
		size_t result_code_len = result_code.length() + 1;
		size_t result_string_len = result_string.length() + 1;
		size_t error_string_len = error_string.length() + 1;
		size_t size = result_code_len + result_string_len + error_string_len;
		char result[size + sizeof(size)];
		char *rp = result;
		memcpy(rp, &size, sizeof(size));
		rp += sizeof(size);
		size += sizeof(size);
		memcpy(rp, result_code.c_str(), result_code_len);
		rp += result_code_len;
		memcpy(rp, result_string.c_str(), result_string_len);
		rp += result_string_len;
		memcpy(rp, error_string.c_str(), error_string_len);
		rp += error_string_len;
		CBasicServer::send_data(lsd->fd, result, size);
	}
	delete lsd;
	Lock();
	if (instance) {
		instance->count--;
		if (!instance->count)
			sem_post(&instance->may_run);
	}
	UnLock();
	pthread_exit(NULL);
}

bool CLuaServer::luaserver_parse_command(CBasicMessage::Header &rmsg __attribute__((unused)), int connfd)
{
	size_t size;

	if (!CBasicServer::receive_data(connfd, &size, sizeof(size))) {
		fprintf(stderr, "%s %s %d: receive_data failed\n", __file__, __func__, __LINE__);
		return true;
	}
	char data[size];
	if (!CBasicServer::receive_data(connfd, data, size)) {
		fprintf(stderr, "%s %s %d: receive_data failed\n", __file__, __func__, __LINE__);
		return true;
	}
	if (data[size - 1]) {
		fprintf(stderr, "%s %s %d: unterminated string\n", __file__, __func__, __LINE__);
		return true;
	}
	std::string luascript;
	if (data[0] == '/')
		luascript = data;
	else {
		luascript = LUAPLUGINDIR "/";
		luascript += data;
		luascript += ".lua";
	}
	if (access(luascript, R_OK)) {
		fprintf(stderr, "%s %s %d: %s not found\n", __file__, __func__, __LINE__, luascript.c_str());
		const char *result_code = "-1";
		const char *result_string = "";
		std::string error_string = luascript + " not found\n";
		size_t result_code_len = strlen(result_code) + 1;
		size_t result_string_len = strlen(result_string) + 1;
		size_t error_string_len = error_string.size() + 1;
		size = result_code_len + result_string_len + error_string_len;
		char result[size + sizeof(size)];
		char *rp = result;
		memcpy(rp, &size, sizeof(size));
		rp += sizeof(size);
		size += sizeof(size);
		memcpy(rp, result_code, result_code_len);
		rp += result_code_len;
		memcpy(rp, result_string, result_string_len);
		rp += result_string_len;
		memcpy(rp, error_string.c_str(), error_string_len);
		rp += error_string_len;
		CBasicServer::send_data(connfd, result, size);
		return true;
	}
	luaserver_data *lsd = new luaserver_data(connfd, luascript);
	char *datap = data;
	while (size > 0) {
		lsd->argv.push_back(std::string(datap));
		size_t len = strlen(datap) + 1;
		datap += len;
		size -= len;
	}

	Lock();
	if (!instance->count)
		sem_wait(&instance->may_run);
	UnLock();

	pthread_t thr;
	pthread_create (&thr, NULL, luaserver_thread, (void *) lsd);
	pthread_detach(thr);

	return true;
}

void *CLuaServer::luaserver_main_thread(void *) {
	set_threadname(__func__);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

	CBasicServer server;
	if (!server.prepare(LUACLIENT_UDS_NAME)) {
		fprintf(stderr, "%s %s %d: prepare failed\n", __file__, __func__, __LINE__);
		pthread_exit(NULL);
	}

	server.run(luaserver_parse_command, LUACLIENT_VERSION);
	pthread_exit(NULL);
}
