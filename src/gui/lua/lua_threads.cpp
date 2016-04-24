/******************************************************************************
* Copyright (c) 2011 by Robert G. Jakabosky <bobby@sharedrealm.com>
* Copyright (c) 2015 M. Liebmann (micha-bbg)
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
//#include <assert.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <driver/rcinput.h>
#include <gui/lua/luainstance.h>
#include "lua_threads.h"

#define assert(x) do {	\
	if (x) \
		fprintf(stderr, "CLLThread:%s:%d assert(%s) failed\n", __func__, __LINE__, #x); \
} while (0)

int __strerror_r(int err, char* buf, size_t len)
{
	memset(buf, '\0', len);
	snprintf(buf, len-1, "%s", strerror(err));
	return 0;
}

CLLThread* CLLThread::getInstance()
{
	static CLLThread* llthreadInst = NULL;

	if (!llthreadInst)
		llthreadInst = new CLLThread();
	return llthreadInst;
}

void CLLThread::llthread_log(lua_State *L, const char *hdr, const char *msg)
{
	int top = lua_gettop(L);
	lua_rawgetp(L, LUA_REGISTRYINDEX, LLTHREAD_LOGGER_HOLDER);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		fputs(hdr,  stderr);
		fputs(msg,  stderr);
		fputc('\n', stderr);
		fflush(stderr);
		return;
	}
	lua_pushstring(L, hdr);
	lua_pushstring(L, msg);
	lua_concat(L, 2);
	lua_pcall(L, 1, 0, 0);
	lua_settop(L, top);
}

int CLLThread::fail(lua_State *L, const char *msg)
{
	lua_pushnil(L);
	lua_pushstring(L, msg);
	return 2;
}

int CLLThread::traceback (lua_State *L)
{
	const char *msg = lua_tostring(L, 1);
	if (msg)
		luaL_traceback(L, L, msg, 1);
	else if (!lua_isnoneornil(L, 1)) {  /* is there an error object? */
		if (!luaL_callmeta(L, 1, "__tostring"))  /* try its 'tostring' metamethod */
			lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

void CLLThread::open_thread_libs(lua_State *L)
{
	/* Not yet tested calling thread from thread out */
	/* LuaThreadsRegister(L); */

	/* luainstance.cpp */
	LuaInstRegisterFunctions(L, true);
}

llthread_child_t *CLLThread::llthread_child_new()
{
	llthread_child_t *_this = ALLOC_STRUCT(llthread_child_t);
	if (!_this) return NULL;

	memset(_this, 0, sizeof(llthread_child_t));

	/* create new lua_State for the thread. */
	/* open standard libraries. */
	_this->L = luaL_newstate();
	open_thread_libs(_this->L);

	return _this;
}

llthread_t *CLLThread::llthread_new()
{
	llthread_t *_this = ALLOC_STRUCT(llthread_t);
	if (!_this) return NULL;

	_this->flags  = FLAG_NONE;
	_this->child  = llthread_child_new();
	if (!_this->child) {
		FREE_STRUCT(_this);
		return NULL;
	}
	return _this;
}

int CLLThread::llthread_push_args(lua_State *L, llthread_child_t *child, int idx, int top)
{
	return llthread_copy_values(L, child->L, idx, top, 1 /* is_arg */);
}

int CLLThread::llthread_push_results(lua_State *L, llthread_child_t *child, int idx, int top)
{
	return llthread_copy_values(child->L, L, idx, top, 0 /* is_arg */);
}

void CLLThread::llthread_validate(llthread_t *_this)
{
	/* describe valid state of llthread_t object
	 * from after create and before destroy
	 */
	if (!IS(_this, STARTED)) {
		assert(!IS(_this, DETACHED));
		assert(!IS(_this, JOINED));
		assert(!IS(_this, JOINABLE));
		return;
	}

	if (IS(_this, DETACHED)) {
		if (!IS(_this, JOINABLE)) assert(_this->child == NULL);
		else assert(_this->child != NULL);
	}
}

llthread_t *CLLThread::llthread_create(lua_State *L, const char *code, size_t code_len)
{
	llthread_t *_this	= llthread_new();
	llthread_child_t *child = _this->child;

	/* load Lua code into child state. */
	int rc = luaL_loadbuffer(child->L, code, code_len, code);
	if (rc != 0) {
		/* copy error message to parent state. */
		size_t len; const char *str = lua_tolstring(child->L, -1, &len);
		if (str != NULL) {
			lua_pushlstring(L, str, len);
		} else {
			/* non-string error message. */
			lua_pushfstring(L, "luaL_loadbuffer() failed to load Lua code: rc=%d", rc);
		}
		llthread_destroy(_this);
		lua_error(L);
		return NULL;
	}

	/* copy extra args from main state to child state. */
	/* Push all args after the Lua code. */
	llthread_push_args(L, child, 3, lua_gettop(L));

	llthread_validate(_this);

	return _this;
}

llthread_t *CLLThread::l_llthread_at (lua_State *L, int i)
{
	llthread_t **_this = (llthread_t **) luaL_checkudata(L, i, LLTHREAD_TAG);
	return *_this;
}

void CLLThread::llthread_child_destroy(llthread_child_t *_this)
{
	lua_close(_this->L);
	FREE_STRUCT(_this);
}

void CLLThread::llthread_cleanup_child(llthread_t *_this)
{
	if (_this->child) {
		llthread_child_destroy(_this->child);
		_this->child = NULL;
	}
}

int CLLThread::llthread_detach(llthread_t *_this)
{
	int rc = 0;

	assert(IS(_this, STARTED));
	assert(_this->child != NULL);

	_this->child = NULL;

	/*we can not detach joined thread*/
	if (IS(_this, JOINED))
		return 0;

	rc = pthread_detach(_this->thread);
	return rc;
}

void* CLLThread::llthread_child_thread_run(void *arg)
{
	llthread_child_t *_this = (llthread_child_t *)arg;
	lua_State *L = _this->L;
	int nargs = lua_gettop(L) - 1;

	/* push traceback function as first value on stack. */
	lua_pushcfunction(_this->L, traceback);
	lua_insert(L, 1);

	_this->status = lua_pcall(L, nargs, LUA_MULTRET, 1);

	/* alwasy print errors here, helps with debugging bad code. */
	if (_this->status != 0) {
		llthread_log(L, "Error from thread: ", lua_tostring(L, -1));
	}

	if (IS(_this, DETACHED) || !IS(_this, JOINABLE)) {
		/* thread is detached, so it must clean-up the child state. */
		llthread_child_destroy(_this);
		_this = NULL;
	}

	return _this;
}

int CLLThread::llthread_start(llthread_t *_this, int start_detached, int joinable)
{
	llthread_child_t *child = _this->child;
	int rc = 0;

	llthread_validate(_this);

	if (joinable) SET(child, JOINABLE);
	if (start_detached) SET(child, DETACHED);

	rc = pthread_create(&(_this->thread), NULL, llthread_child_thread_run, child);

	if (rc == 0) {
		SET(_this, STARTED);
		if (joinable) SET(_this, JOINABLE);
		if (start_detached) SET(_this, DETACHED);
		if ((start_detached)&&(!joinable)) {
			rc = llthread_detach(_this);
		}
	}

	llthread_validate(_this);

	return rc;
}

int CLLThread::llthread_cancel(llthread_t *_this)
{
	llthread_validate(_this);

	if (IS(_this, JOINED)) {
		return JOIN_OK;
	} else {
		int rc = pthread_cancel(_this->thread);
		if (rc == 0) {
			return JOIN_ETIMEDOUT;
		}

		if (rc != ESRCH) {
			/*@fixme what else it can be ?*/
			return rc;
		}

		return JOIN_OK;
	}
}

int CLLThread::llthread_join(llthread_t *_this, join_timeout_t timeout)
{
	llthread_validate(_this);

	if (IS(_this, JOINED)) {
		return JOIN_OK;
	} else {
		int rc;
		if (timeout == 0) {
			rc = pthread_kill(_this->thread, 0);
			if (rc == 0) { /* still alive */
				return JOIN_ETIMEDOUT;
			}

			if (rc != ESRCH) {
				/*@fixme what else it can be ?*/
				return rc;
			}

			/*thread dead so we call join to free pthread_t struct */
		}

		/* @todo use pthread_tryjoin_np/pthread_timedjoin_np to support timeout */

		/* then join the thread. */
		rc = pthread_join(_this->thread, NULL);
		if ((rc == 0) || (rc == ESRCH)) {
			SET(_this, JOINED);
			rc = JOIN_OK;
		}

		llthread_validate(_this);

		return rc;
	}
}

void CLLThread::llthread_destroy(llthread_t *_this)
{
	do {
		/* thread not started */
		if (!IS(_this, STARTED)) {
			llthread_cleanup_child(_this);
			break;
		}

		/* DETACHED */
		if (IS(_this, DETACHED)) {
			if (IS(_this, JOINABLE)) {
				llthread_detach(_this);
			}
			break;
		}

		/* ATTACHED */
		if (!IS(_this, JOINED)) {
			llthread_join(_this, INFINITE_JOIN_TIMEOUT);
			if (!IS(_this, JOINED)) {
				/* @todo use current lua state to logging */
				/*
				 * char buf[ERROR_LEN];
				 * strerror_r(errno, buf, ERROR_LEN);
				 * llthread_log(L, "Error can not join thread on gc: ", buf);
				 */
			}
		}
		if (IS(_this, JOINABLE)) {
			llthread_cleanup_child(_this);
		}

	} while (0);

	FREE_STRUCT(_this);
}

int CLLThread::llthread_alive(llthread_t *_this)
{
	llthread_validate(_this);

	if (IS(_this, JOINED)) {
		return JOIN_OK;
	} else {
		int rc = pthread_kill(_this->thread, 0);
		if (rc == 0) { /* still alive */
			return JOIN_ETIMEDOUT;
		}

		if (rc != ESRCH) {
			/*@fixme what else it can be ?*/
			return rc;
		}
		return JOIN_OK;
	}
}
