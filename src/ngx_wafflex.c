#include <ngx_wafflex.h>
#include <util/ipc.h>
#include <util/shmem.h>
#include <ruleset/ruleset.h>

#include "ngx_wafflex_nginx_lua_scripts.h"

#include <assert.h>

#define __wfx_lua_loadscript(lua_state, name, wherefrom) \
  luaL_loadbuffer(lua_state, wherefrom.name.script, strlen(wherefrom.name.script), #name); \
  lua_ngxcall(lua_state, 0, LUA_MULTRET)

#define wfx_lua_loadscript(lua_state, name)   \
  __wfx_lua_loadscript(lua_state, name, wfx_lua_scripts)
  
#define wfx_lua_module_loadscript(lua_state, name)   \
  __wfx_lua_loadscript(lua_state, name, wfx_module_lua_scripts)

ngx_int_t luaL_checklstring_as_ngx_str(lua_State *L, int n, ngx_str_t *str) {
  size_t         data_sz;
  const char    *data = luaL_checklstring(L, n, &data_sz);
  
  str->data = (u_char *)data;
  str->len = data_sz;
  return NGX_OK;
}

static ipc_t        *ipc = NULL;
static lua_State    *Lua = NULL;

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
      sprintf(buf, "%s", typename);
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

static int wfx_lua_traceback(lua_State *L) {
  if (!lua_isstring(L, -1)) { /* 'message' not a string? */
    return 1;  /* keep it intact */
  }

  lua_getglobal(L, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return 1;
  }

  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return 1;
  }

  lua_pushvalue(L, 1);  /* pass error message */
  lua_pushinteger(L, 2);  /* skip this function and traceback */
  lua_call(L, 2, 1);  /* call debug.traceback */
  return 1;
}


void lua_ngxcall(lua_State *L, int nargs, int nresults) {
  int rc;
  lua_pushcfunction(L, wfx_lua_traceback);
  lua_insert(L, 1);
  
  rc = lua_pcall(L, nargs, nresults, 1);
  if (rc != 0) {
    ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "<wafflex.lua>: %s", lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  
  lua_remove(L, 1);
}


static int wfx_require_module(lua_State *L) {
  wfx_module_lua_script_t *script;
  ngx_str_t                name;
  luaL_checklstring_as_ngx_str(L, 1, &name);
  WFX_MODULE_LUA_SCRIPTS_EACH(script) {
    if(strcmp(script->name, (char *)name.data) == 0) {
      luaL_loadbuffer(L, script->script, strlen(script->script), script->name);
      lua_ngxcall(L, 0, 1);
      return 1;
    }
  }
  ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "notfound");
  lua_pushnil(L);
  return 1;
}

static int wfx_init_bind_lua(lua_State *L) {
  //ruleset bindings
  lua_pushcfunction(L, wfx_ruleset_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  return 0;
}

static ngx_int_t ngx_wafflex_init_postconfig(ngx_conf_t *cf) {  
  
  Lua = luaL_newstate();
  luaL_openlibs(Lua);
  
  wfx_lua_loadscript(Lua, init);  
  lua_pushcfunction(Lua, wfx_require_module);
  lua_pushcfunction(Lua, wfx_init_bind_lua);
  
  lua_ngxcall(Lua, 2, 0);
  
  return NGX_OK;
}

static void ngx_wafflex_ipc_alert_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data) {
  //do nothing -- for now
}


static ngx_int_t ngx_wafflex_init_module(ngx_cycle_t *cycle) {
  
  if(ipc) { //ipc already exists. destroy it!
    ipc_destroy(ipc);
  }
  ipc = ipc_init_module("wafflex", cycle);
  //ipc->track_stats = 1;
  ipc_set_alert_handler(ipc, ngx_wafflex_ipc_alert_handler);
  
  return NGX_OK;
}

static ngx_int_t ngx_wafflex_init_worker(ngx_cycle_t *cycle) {
  return ipc_init_worker(ipc, cycle);
}

static void ngx_wafflex_exit_worker(ngx_cycle_t *cycle) {
  ipc_destroy(ipc);
}

static void ngx_wafflex_exit_master(ngx_cycle_t *cycle) {
  ipc_destroy(ipc);
}

static ngx_command_t  ngx_wafflex_commands[] = {
  ngx_null_command
};

static ngx_http_module_t  ngx_wafflex_ctx = {
  NULL,                          /* preconfiguration */
  ngx_wafflex_init_postconfig,   /* postconfiguration */
  NULL,                          /* create main configuration */
  NULL,                          /* init main configuration */
  NULL,                          /* create server configuration */
  NULL,                          /* merge server configuration */
  NULL,                          /* create location configuration */
  NULL,                          /* merge location configuration */
};

ngx_module_t  ngx_wafflex_module = {
  NGX_MODULE_V1,
  &ngx_wafflex_ctx,              /* module context */
  ngx_wafflex_commands,          /* module directives */
  NGX_HTTP_MODULE,               /* module type */
  NULL,                          /* init master */
  ngx_wafflex_init_module,       /* init module */
  ngx_wafflex_init_worker,       /* init process */
  NULL,                          /* init thread */
  NULL,                          /* exit thread */
  ngx_wafflex_exit_worker,       /* exit process */
  ngx_wafflex_exit_master,       /* exit master */
  NGX_MODULE_V1_PADDING
};
