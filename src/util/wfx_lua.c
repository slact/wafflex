#include <ngx_wafflex.h>
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

#if LUA_VERSION_NUM <= 501
#define COMPAT53_LEVELS1 12  /* size of the first part of the stack */
#define COMPAT53_LEVELS2 10 /* size of the second part of the stack */
static int compat53_countlevels (lua_State *L) {
  lua_Debug ar;
  int li = 1, le = 1;
  /* find an upper bound */
  while (lua_getstack(L, le, &ar)) { li = le; le *= 2; }
  /* do a binary search */
  while (li < le) {
    int m = (li + le)/2;
    if (lua_getstack(L, m, &ar)) li = m + 1;
    else le = m;
  }
  return le - 1;
}

void luaL_traceback (lua_State *L, lua_State *L1,
                                  const char *msg, int level) {
  lua_Debug ar;
  int top = lua_gettop(L);
  int numlevels = compat53_countlevels(L1);
  int mark = (numlevels > COMPAT53_LEVELS1 + COMPAT53_LEVELS2) ? COMPAT53_LEVELS1 : 0;
  if (msg) lua_pushfstring(L, "%s\n", msg);
  lua_pushliteral(L, "stack traceback:");
  while (lua_getstack(L1, level++, &ar)) {
    if (level == mark) {  /* too many levels? */
      lua_pushliteral(L, "\n\t...");  /* add a '...' */
      level = numlevels - COMPAT53_LEVELS2;  /* and skip to last ones */
    }
    else {
      lua_getinfo(L1, "Slnt", &ar);
      lua_pushfstring(L, "\n\t%s:", ar.short_src);
      if (ar.currentline > 0)
        lua_pushfstring(L, "%d:", ar.currentline);
      lua_pushliteral(L, " in ");
      compat53_pushfuncname(L, &ar);
      lua_concat(L, lua_gettop(L) - top);
    }
  }
  lua_concat(L, lua_gettop(L) - top);
}
#endif

int wfx_lua_resume(lua_State *thread, int nargs) {
  int rc;
  const char *errmsg;
#if LUA_VERSION_NUM <= 501
  rc = lua_resume(thread, nargs);
#else
  rc = lua_resume(thread, NULL, nargs);
#endif
  switch(rc) {
    case LUA_OK:
    case LUA_YIELD:
      return rc;
    default:
      errmsg = lua_tostring(thread, -1);
      luaL_traceback(thread, thread, errmsg, 1);
      ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "<wafflex:lua>: %s", lua_tostring(thread, -1));
      lua_pop(thread, 1);
      lua_gc(thread, LUA_GCCOLLECT, 0);
      return rc;
  }
  return rc;
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

typedef struct {
  lua_State  *L;
  int         ref;
} lua_timeout_data_t;

void lua_timeout_callback(void *pd) {
  lua_timeout_data_t *d = pd;
  wfx_lua_pushfromref(d->L, d->ref);
  if(lua_isthread(d->L, -1)) {
    lua_State *thread = lua_tothread(d->L, -1);
    wfx_lua_resume(thread, 0);
  }
  else {
    lua_ngxcall(d->L, 0, 0);
  }
  wfx_lua_unref(d->L, d->ref);
}

int wfx_lua_timeout(lua_State *L) {
  ngx_event_t        *ev;
  lua_timeout_data_t *d;
  int                ref;
  int                yield = 0;
  ngx_msec_t         delay;
  int                nargs = lua_gettop(L);
  
  if(nargs == 0) {
    luaL_error(L, "you're doing it wrong");
    return 0;
  }
  delay = lua_tonumber(L, 1);
  
  if(nargs == 1) { //must be in a coroutine. otherwise this is meaningless
    lua_getglobal(L, "coroutine");
    lua_getfield(L, -1, "running");
    lua_call(L, 0, 1);
    if(lua_isthread(L, -1)) {
      //we're in a coroutine. great!
      ref = wfx_lua_ref(L, -1);
      yield = 1;
    }
    else {
      luaL_error(L, "can't timeout without a callback and not in a coroutine");
      return 0;
    }
    lua_pop(L, 2);
  }
  else {
    if(lua_isfunction(L, 2) || lua_isthread(L, 2)) {
      ref = wfx_lua_ref(L, 2);
    }
    else {
      luaL_error(L, "given something not a coroutine or function. wtf?");
      return 0;
    }
  }
  
  ev = wfx_add_oneshot_timer(lua_timeout_callback, sizeof(*d), delay);
  if(!ev) {
    wfx_lua_unref(L, ref);
    luaL_error(L, "unable to create oneshot timer");
    return 0;
  }
  
  d = ev->data;
  d->L = L;
  d->ref = ref;
  
  if(yield) {
    return lua_yield(L, 0);
  }
  else {
    lua_pushboolean(L, 1);
    return 1;
  }
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
