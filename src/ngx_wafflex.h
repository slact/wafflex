#ifndef NGX_WAFFLEX_H
#define NGX_WAFFLEX_H

#include <nginx.h>
#include <ngx_http.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define DEBUG_ON

#ifdef DEBUG_ON
#define DBG(fmt, args...) ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "wafflex:" fmt, ##args)
#else
#define DBG(fmt, args...) 
#endif
#define ERR(fmt, args...) ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "wafflex:" fmt, ##args)

#ifndef container_of
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

void *wfx_shm_alloc(size_t sz);
void *wfx_shm_calloc(size_t sz);
void  wfx_shm_free(void *ptr);

extern ngx_module_t ngx_wafflex_module;

void lua_ngxcall(lua_State *L, int nargs, int nresults);
//debug stuff
void lua_printstack(lua_State *L);
size_t wfx_lua_tablesize(lua_State *L, int index);
ngx_int_t luaL_checklstring_as_ngx_str(lua_State *L, int n, ngx_str_t *str);
void lua_mm(lua_State *L, int index);

#define wfx_lua_register(L, func) \
  wfx_lua_register_named(L, func, #func)

#define wfx_lua_register_named(L, func, name) \
  (lua_pushcfunction(L, func), lua_pushvalue(L, -1), lua_setglobal(L, name))

//possibly missing stuff
#if LUA_VERSION_NUM <= 501
lua_Integer luaL_len (lua_State *L, int i);
#endif

#endif //NGX_WAFFLEX_H
