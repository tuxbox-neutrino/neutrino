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

cat <<EOT > /lib/tuxbox/luaplugins/test.lua
for i,v in ipairs(arg) do
	print(tostring(i) .. "\t" .. tostring(v))
end
return "ok"
EOT

followed by luaclient test a b c d
#endif

#include <config.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <unistd.h>

#include <global.h>
#include <neutrino.h>
#include <system/helpers.h>
#include <system/set_threadname.h>
#include <luaclient/luaclient.h>
#include <gui/luainstance.h>

#include "luaserver.h"

static CLuaServer *instance = NULL;

CLuaServer *CLuaServer::getInstance()
{
	if (!instance)
		instance = new CLuaServer();
	return instance;
}

void CLuaServer::destroyInstance()
{
	if (instance) {
		delete instance;
		instance = NULL;
	}
}

CLuaServer::CLuaServer()
{
	count = 0;

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
	pthread_mutex_init(&mutex, &attr);

	sem_init(&may_run, 0, 0);
	sem_init(&did_run, 0, 0);

	pthread_create (&thr, NULL, luaserver_main_thread, (void *) NULL);
}

CLuaServer::~CLuaServer()
{
	sem_destroy(&may_run);
	sem_destroy(&did_run);
	instance = NULL;
	pthread_join(thr, NULL);
}

void CLuaServer::UnBlock()
{
	sem_post(&may_run);
}

bool CLuaServer::Block(const neutrino_msg_t msg, const neutrino_msg_data_t data)
{
	sem_wait(&may_run);
	if (!sem_trywait(&did_run)) {
		if (msg != CRCInput::RC_timeout)
			g_RCInput->postMsg(msg, data);
		while (!sem_trywait(&did_run));
		return true;
	}
	return false;
}

class luaserver_data
{
	public:
		int fd;
		std::vector<std::string> argv;
		std::string script;

		luaserver_data(int _fd, std::string &_script) {
			fd = dup(_fd);
			fcntl(fd, F_SETFD, FD_CLOEXEC);
			script = _script;
		}
		~luaserver_data(void) {
			close(fd);
		}
};

void CLuaServer::Lock(void)
{
	pthread_mutex_lock(&instance->mutex);
}

void CLuaServer::UnLock(void)
{
	pthread_mutex_unlock(&instance->mutex);
}

void *CLuaServer::luaserver_thread(void *arg) {
	set_threadname(__func__);
	Lock();
	instance->count++;
	UnLock();
	sem_post(&instance->did_run);

	luaserver_data *lsd = (class luaserver_data *)arg;

	CLuaInstance lua;
	std::string result_code;
	std::string result_string;
	std::string error_string;
	lua.runScript(lsd->script.c_str(), &lsd->argv, &result_code, &result_string, &error_string);
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

	delete lsd;
	Lock();
	instance->count--;
	if (!instance->count)
		sem_post(&instance->may_run);
	UnLock();
	pthread_exit(NULL);
}

bool CLuaServer::luaserver_parse_command(CBasicMessage::Header &rmsg __attribute__((unused)), int connfd)
{
	size_t size;

	if (!CBasicServer::receive_data(connfd, &size, sizeof(size))) {
		fprintf(stderr, "%s %s %d: receive_data failed\n", __FILE__, __func__, __LINE__);
		return true;
	}
	char data[size];
	if (!CBasicServer::receive_data(connfd, data, size)) {
		fprintf(stderr, "%s %s %d: receive_data failed\n", __FILE__, __func__, __LINE__);
		return true;
	}
	if (data[size - 1]) {
		fprintf(stderr, "%s %s %d: unterminated string\n", __FILE__, __func__, __LINE__);
		return true;
	}
	std::string luascript(LUAPLUGINDIR "/");
	luascript += data;
	luascript += ".lua";
	if (access(luascript, R_OK)) {
		fprintf(stderr, "%s %s %d: %s not found\n", __FILE__, __func__, __LINE__, luascript.c_str());
		const char *result_code = "-1";
		const char *result_string = "";
		std::string error_string = luascript + " not found\n";
		size_t result_code_len = strlen(result_code) + 1;
		size_t result_string_len = strlen(result_string) + 1;
		size_t error_string_len = strlen(error_string.c_str()) + 1;
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

	CBasicServer server;
	if (!server.prepare(LUACLIENT_UDS_NAME)) {
		fprintf(stderr, "%s %s %d: prepare failed\n", __FILE__, __func__, __LINE__);
		pthread_exit(NULL);
	}


	server.run(luaserver_parse_command, LUACLIENT_VERSION);
	pthread_exit(NULL);
}
