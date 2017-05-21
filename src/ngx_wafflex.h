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

extern ngx_module_t ngx_wafflex_module;

void lua_ngxcall(lua_State *L, int nargs, int nresults);
//debug stuff
void lua_printstack(lua_State *L);

#endif //NGX_WAFFLEX_H
