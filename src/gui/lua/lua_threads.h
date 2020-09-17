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

#ifndef _LLTHREAD_H_
#define _LLTHREAD_H_

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
#include <pthread.h>
#include <config.h>

/* wrap strerror_r(). */
#ifndef strerror_r
#define strerror_r __strerror_r
#endif

#if LUA_COMPAT_5_2
void lua_rawsetp (lua_State *L, int i, const void *p);
#endif

#define OS_THREAD_RETURN void *
#define INFINITE_JOIN_TIMEOUT -1
#define JOIN_OK 0
#define JOIN_ETIMEDOUT ETIMEDOUT
typedef int join_timeout_t;
typedef pthread_t os_thread_t;

#define ERROR_LEN 1024

#define flags_t unsigned char

#define FLAG_NONE      (flags_t)0
#define FLAG_STARTED   (flags_t)1<<0
#define FLAG_DETACHED  (flags_t)1<<1
#define FLAG_JOINED    (flags_t)1<<2
#define FLAG_JOINABLE  (flags_t)1<<3

/*At least one flag*/
#define FLAG_IS_SET(O, F) (O->flags & (flags_t)(F))
#define FLAG_SET(O, F) O->flags |= (flags_t)(F)
#define FLAG_UNSET(O, F) O->flags &= ~((flags_t)(F))
#define IS(O, F) FLAG_IS_SET(O, FLAG_##F)
#define SET(O, F) FLAG_SET(O, FLAG_##F)

#define ALLOC_STRUCT(S) (S*)calloc(1, sizeof(S))
#define FREE_STRUCT(O) free(O)

#define LLTHREAD_NAME "threads"
#define LLTHREAD_TAG LLTHREAD_NAME
#define LLTHREAD_LOGGER_HOLDER LLTHREAD_NAME " logger holder"

typedef struct llthread_child_t {
	lua_State*  L;
	int         status;
	flags_t     flags;
} llthread_child_t;

typedef struct llthread_t {
	llthread_child_t* child;
	os_thread_t       thread;
	flags_t           flags;
} llthread_t;

typedef struct {
	lua_State* from_L;
	lua_State* to_L;
	int        has_cache;
	int        cache_idx;
	int        is_arg;
} llthread_copy_state;

#if LUA_VERSION_NUM >= 503 /* Lua 5.3 */

#ifndef luaL_optint
# define luaL_optint luaL_optinteger
#endif

#ifndef luaL_checkint
# define luaL_checkint luaL_checkinteger
#endif

#endif /* Lua 5.3 */

int __strerror_r(int err, char* buf, size_t len);

class CLLThread
{
	public:
		CLLThread() {};
//		~CLLThread() {};
		static CLLThread* getInstance();
		static void LuaThreadsRegister(lua_State *L);

	private:
		static void llthread_log(lua_State *L, const char *hdr, const char *msg);
		static int fail(lua_State *L, const char *msg);
		static int traceback (lua_State *L);
		static void open_thread_libs(lua_State *L);
		static llthread_child_t *llthread_child_new();
		static llthread_t *llthread_new();
		static int llthread_push_args(lua_State *L, llthread_child_t *child, int idx, int top);
		static int llthread_push_results(lua_State *L, llthread_child_t *child, int idx, int top);
		static void llthread_validate(llthread_t *_this);
		static llthread_t *llthread_create(lua_State *L, const char *code, size_t code_len);
		static llthread_t *l_llthread_at (lua_State *L, int i);
		static void llthread_child_destroy(llthread_child_t *_this);
		static void llthread_cleanup_child(llthread_t *_this);
		static int llthread_detach(llthread_t *_this);
		static void* llthread_child_thread_run(void *arg);
		static int llthread_start(llthread_t *_this, int start_detached, int joinable);
		static int llthread_cancel(llthread_t *_this);
		static int llthread_join(llthread_t *_this, join_timeout_t timeout);
		static void llthread_destroy(llthread_t *_this);
		static int llthread_alive(llthread_t *_this);

		/* copy.cpp */
		static int llthread_copy_table_from_cache(llthread_copy_state *state, int idx);
		static int llthread_copy_value(llthread_copy_state *state, int depth, int idx);
		static int llthread_copy_values(lua_State *from_L, lua_State *to_L, int idx, int top, int is_arg);

		/* lua_functions.cpp */
		static int l_llthread_new(lua_State *L);
		static int l_llthread_start(lua_State *L);
		static int l_llthread_cancel(lua_State *L);
		static int l_llthread_join(lua_State *L);
		static int l_llthread_alive(lua_State *L);
		static int l_llthread_set_logger(lua_State *L);
		static int l_llthread_started(lua_State *L);
		static int l_llthread_detached(lua_State *L);
		static int l_llthread_joinable(lua_State *L);
		static int l_llthread_delete(lua_State *L);
};

#endif // _LLTHREAD_H_
