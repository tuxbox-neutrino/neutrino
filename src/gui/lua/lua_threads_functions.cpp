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
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "lua_threads.h"

void CLLThread::LuaThreadsRegister(lua_State *L)
{
	luaL_Reg meth[] = {
		{ "new",        CLLThread::l_llthread_new        },
		{ "start",      CLLThread::l_llthread_start      },
		{ "cancel",     CLLThread::l_llthread_cancel     },
		{ "join",       CLLThread::l_llthread_join       },
#if 0
		/* Fix me */
		{ "set_logger", CLLThread::l_llthread_set_logger },
#endif
		{ "started",    CLLThread::l_llthread_started    },
		{ "detached",   CLLThread::l_llthread_detached   },
		{ "joinable",   CLLThread::l_llthread_joinable   },
		{ "__gc",       CLLThread::l_llthread_delete     },
		{ NULL, NULL }
	};

	luaL_newmetatable(L, LLTHREAD_TAG);
	luaL_setfuncs(L, meth, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -1, "__index");
	lua_setglobal(L, LLTHREAD_TAG);
}

int CLLThread::l_llthread_new(lua_State *L)
{
	size_t lua_code_len; const char *lua_code = luaL_checklstring(L, 1, &lua_code_len);

	llthread_t **_this = (llthread_t **) lua_newuserdata(L, sizeof(llthread_t *));
	luaL_getmetatable(L, LLTHREAD_TAG);
	lua_setmetatable(L, -2);
	lua_insert(L, 2); /*move self prior args*/
	*_this = llthread_create(L, lua_code, lua_code_len);

	lua_settop(L, 2);
	return 1;
}

int CLLThread::l_llthread_start(lua_State *L)
{
	llthread_t *_this = l_llthread_at(L, 1);
	int start_detached = lua_toboolean(L, 2);
	int joinable, rc;

	if (!lua_isnone(L, 3))
		joinable = lua_toboolean(L, 3);
	else
		joinable = start_detached ? 0 : 1;

	if (IS(_this, STARTED)) {
		return fail(L, "Thread already started.");
	}

	rc = llthread_start(_this, start_detached, joinable);
	if (rc != 0) {
		char buf[ERROR_LEN];
		strerror_r(errno, buf, ERROR_LEN);
		return fail(L, buf);
	}

	lua_settop(L, 1); // return _this
	return 1;
}

int CLLThread::l_llthread_cancel(lua_State *L)
{
	llthread_t *_this = l_llthread_at(L, 1);
	/*  llthread_child_t *child = _this->child; */
	int rc;

	if (!IS(_this, STARTED )) {
		return fail(L, "Can't cancel a thread that hasn't be started.");
	}
	if ( IS(_this, DETACHED)) {
		return fail(L, "Can't cancel a thread that has been detached.");
	}
	if ( IS(_this, JOINED  )) {
		return fail(L, "Can't cancel a thread that has already been joined.");
	}

	/* cancel the thread. */
	rc = llthread_cancel(_this);

	if ( rc == JOIN_ETIMEDOUT ) {
		lua_pushboolean(L, 1);
		return 1;
	}

	if (rc == JOIN_OK) {
		lua_pushboolean(L, 0);
		return 1;
	}

	char buf[ERROR_LEN];
	strerror_r(errno, buf, ERROR_LEN);
	/* llthread_cleanup_child(_this); */
	return fail(L, buf);
}

int CLLThread::l_llthread_join(lua_State *L)
{
	llthread_t *_this = l_llthread_at(L, 1);
	llthread_child_t *child = _this->child;
	int rc;

	if (!IS(_this, STARTED )) {
		return fail(L, "Can't join a thread that hasn't be started.");
	}
	if ( IS(_this, DETACHED) && !IS(_this, JOINABLE)) {
		return fail(L, "Can't join a thread that has been detached.");
	}
	if ( IS(_this, JOINED  )) {
		return fail(L, "Can't join a thread that has already been joined.");
	}

	/* join the thread. */
	rc = llthread_join(_this, luaL_optint(L, 2, INFINITE_JOIN_TIMEOUT));

	if (child && IS(_this, JOINED)) {
		int top;

		if (IS(_this, DETACHED) || !IS(_this, JOINABLE)) {
			/*child lua state has been destroyed by child thread*/
			/*@todo return thread exit code*/
			lua_pushboolean(L, 1);
			lua_pushnumber(L, 0);
			return 2;
		}

		/* copy values from child lua state */
		if (child->status != 0) {
			const char *err_msg = lua_tostring(child->L, -1);
			lua_pushboolean(L, 0);
			lua_pushfstring(L, "Error from child thread: %s", err_msg);
			top = 2;
		} else {
			lua_pushboolean(L, 1);
			top = lua_gettop(child->L);
			/* return results to parent thread. */
			llthread_push_results(L, child, 2, top);
		}

//		llthread_cleanup_child(_this);
		return top;
	}

	if ( rc == JOIN_ETIMEDOUT ) {
		return fail(L, "timeout");
	}

	char buf[ERROR_LEN];
	strerror_r(errno, buf, ERROR_LEN);
	/* llthread_cleanup_child(_this); */
	return fail(L, buf);
}

int CLLThread::l_llthread_alive(lua_State *L)
{
	llthread_t *_this = l_llthread_at(L, 1);
	/* llthread_child_t *child = _this->child; */
	int rc;

	if (!IS(_this, STARTED )) {
		return fail(L, "Can't join a thread that hasn't be started.");
	}
	if ( IS(_this, DETACHED) && !IS(_this, JOINABLE)) {
		return fail(L, "Can't join a thread that has been detached.");
	}
	if ( IS(_this, JOINED  )) {
		return fail(L, "Can't join a thread that has already been joined.");
	}

	/* join the thread. */
	rc = llthread_alive(_this);

	if ( rc == JOIN_ETIMEDOUT ) {
		lua_pushboolean(L, 1);
		return 1;
	}

	if (rc == JOIN_OK) {
		lua_pushboolean(L, 0);
		return 1;
	}

	char buf[ERROR_LEN];
	strerror_r(errno, buf, ERROR_LEN);
	/* llthread_cleanup_child(_this); */
	return fail(L, buf);
}

int CLLThread::l_llthread_set_logger(lua_State *L)
{
	lua_settop(L, 1);
	luaL_argcheck(L, lua_isfunction(L, 1), 1, "function expected");
	lua_rawsetp(L, LUA_REGISTRYINDEX, LLTHREAD_LOGGER_HOLDER);
	return 0;
}

int CLLThread::l_llthread_started(lua_State *L)
{
	llthread_t *_this = l_llthread_at(L, 1);
	lua_pushboolean(L, IS(_this, STARTED)?1:0);
	return 1;
}

int CLLThread::l_llthread_detached(lua_State *L)
{
	llthread_t *_this = l_llthread_at(L, 1);
	lua_pushboolean(L, IS(_this, DETACHED)?1:0);
	return 1;
}

int CLLThread::l_llthread_joinable(lua_State *L)
{
	llthread_t *_this = l_llthread_at(L, 1);
	lua_pushboolean(L, IS(_this, JOINABLE)?1:0);
	return 1;
}

int CLLThread::l_llthread_delete(lua_State *L)
{
	llthread_t **pthis = (llthread_t **)luaL_checkudata(L, 1, LLTHREAD_TAG);
	luaL_argcheck (L, pthis != NULL, 1, "thread expected");
	if (*pthis == NULL) return 0;
	llthread_destroy(*pthis);
	*pthis = NULL;

	return 0;
}
