#include "wfx_lua.h"
#include <assert.h>

//lua helpers
size_t wfx_lua_tablesize(lua_State *L, int index) {
  int n = 0;
  index = lua_absindex(L, index);
  lua_pushnil(L);  // first key 
  while (lua_next(L, index) != 0) {
    n++;
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  return n;
}

static int wfx_lua_traceback(lua_State *L) {
  if (!lua_isstring(L, -1)) { /* 'message' not a string? */
    return 1;  /* keep it intact */
  }

  lua_getglobal(L, "debug");
  lua_getfield(L, -1, "traceback");

  lua_pushvalue(L, 1);  /* pass error message */
  lua_pushinteger(L, 2);  /* skip this function and traceback */
  lua_call(L, 2, 1);  /* call debug.traceback */
  return 1;
}

void lua_ngxcall(lua_State *L, int nargs, int nresults) {
  int rc;
  if(!lua_isfunction(L, -(nargs+1))) {
    lua_printstack(L);
    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "nargs: %i", nargs);
    assert(lua_isfunction(L, -(nargs+1)));
  }
  
  lua_pushcfunction(L, wfx_lua_traceback);
  lua_insert(L, 1);
  
  rc = lua_pcall(L, nargs, nresults, 1);
  if (rc != 0) {
    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "<wafflex:lua>: %s", lua_tostring(L, -1));
    lua_pop(L, 1);
    lua_gc(L, LUA_GCCOLLECT, 0);
#ifdef WFX_CRASH_ON_LUA_ERROR
    raise(SIGABRT);
#endif
  }
  lua_remove(L, 1);
}


int wfx_lua_ref(lua_State *L, int index) {
  lua_pushvalue(L, index);
  return luaL_ref(L, LUA_REGISTRYINDEX);
}
int wfx_lua_pushfromref(lua_State *L, int ref) {
  return lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
}
void wfx_lua_unref(lua_State *L, int index) {
  luaL_unref(L, LUA_REGISTRYINDEX, index);
}

int wfx_lua_getfunction(lua_State *L, const char *name) {
  lua_getglobal(L, name);
  assert(lua_isfunction(L, -1));
  return 1;
}

size_t wfx_lua_len(lua_State *L, int index) {
#if LUA_VERSION_NUM <= 501
  return lua_objlen(L, index);
#else
  return luaL_len(L, index);
#endif
}

int wfx_luaL_loadbuffer(lua_State *L, const char *buff, size_t sz, const char *name, const char *fmt) {
  char     scriptname[512];
  snprintf(scriptname, 512, "=%s.lua", name);
  
  return luaL_loadbuffer(L, buff, sz, scriptname);
}

void lua_pushngxstr(lua_State *L, ngx_str_t *str) {
  lua_pushlstring(L, (const char *)str->data, str->len);
}

//wafflex stuff

void wfx_lua_binding_set(lua_State *L, wfx_binding_t *binding) {
  char     buf[255];
  
  lua_getglobal(L, "Binding");
  lua_getfield(L, -1, "set");
  lua_remove(L, -2);
  
  lua_pushstring(L, binding->name);
  if(binding->create) {
    snprintf(buf, 255, "%s create", binding->name);
    wfx_lua_register_named(L, binding->create, buf);
  }
  else {
    lua_pushnil(L);
  }
  if(binding->replace) {
    snprintf(buf, 255, "%s replace", binding->name);
    wfx_lua_register_named(L, binding->replace, buf);
  }
  else {
    lua_pushnil(L);
  }
  if(binding->update) {
    snprintf(buf, 255, "%s update", binding->name);
    wfx_lua_register_named(L, binding->update, buf);
  }
  else {
    lua_pushnil(L);
  }
  if(binding->delete) {
    snprintf(buf, 255, "%s delete", binding->name);
    wfx_lua_register_named(L, binding->delete, buf);
  }
  else {
    lua_pushnil(L);
  }
  
  lua_ngxcall(L,5,0);
}

//debug stuff
void lua_mm(lua_State *L, int index) {
  index = lua_absindex(L, index);
  lua_getglobal (L, "mm");
  lua_pushvalue(L, index);
  lua_call(L, 1, 0);
}

static char *lua_dbgval(lua_State *L, int n) {
  static char buf[255];
  int         type = lua_type(L, n);
  const char *typename = lua_typename(L, type);
  const char *str;
  lua_Number  num;
  
  
  switch(type) {
    case LUA_TNUMBER:
      num = lua_tonumber(L, n);
      sprintf(buf, "%s: %f", typename, num);
      break;
    case LUA_TBOOLEAN:
      sprintf(buf, "%s: %s", typename, lua_toboolean(L, n) ? "true" : "false");
      break;
    case LUA_TSTRING:
      str = lua_tostring(L, n);
      sprintf(buf, "%s: %.50s%s", typename, str, strlen(str) > 50 ? "..." : "");
      break;
    default:
      lua_getglobal(L, "tostring");
      lua_pushvalue(L, n);
      lua_call(L, 1, 1);
      str = lua_tostring(L, -1);
      sprintf(buf, "%s", str);
      lua_pop(L, 1);
  }
  return buf;
}

void lua_printstack(lua_State *L) {
  int n, top = lua_gettop(L);
  ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "lua stack:");
  for(n=top; n>0; n--) {
    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "  [%i]: %s", n, lua_dbgval(L, n));
  }
}
