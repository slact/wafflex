#ifndef WFX_LUA_H
#define WFX_LUA_H
#include <nginx.h>
#include <ngx_http.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <ngx_wafflex_types.h>

#define wfx_lua_register(L, func) \
  wfx_lua_register_named(L, func, #func)

#define wfx_lua_register_named(L, func, name) \
  (lua_pushcfunction(L, func), lua_pushvalue(L, -1), lua_setglobal(L, name))
  
//generic lua helpers
size_t wfx_lua_tablesize(lua_State *L, int index);
void lua_ngxcall(lua_State *L, int nargs, int nresults);
int wfx_lua_getref(lua_State *L, int index);
size_t wfx_lua_len(lua_State *L, int index);
int wfx_luaL_loadbuffer(lua_State *L, const char *buff, size_t sz, const char *name, const char *fmt);

//wafflex-specific lua stuff
int wfx_lua_require_module(lua_State *L);
void wfx_lua_binding_set(lua_State *L, wfx_binding_t *binding);

//debuggy stuff
void lua_mm(lua_State *L, int index);
void lua_printstack(lua_State *L);
#endif //WFX_LUA_H