#include <ngx_wafflex.h>
#include <util/ipc.h>
#include <util/shmem.h>
#include <ruleset/ruleset.h>
#include <util/wfx_str.h>
#include <ngx_wafflex_nginx_lua_scripts.h>

#define __wfx_lua_loadscript(lua_state, name, wherefrom) \
  wfx_luaL_loadbuffer(lua_state, wherefrom.name.script, strlen(wherefrom.name.script), #name, "=%s.lua"); \
  lua_ngxcall(lua_state, 0, LUA_MULTRET)

#define wfx_lua_loadscript(lua_state, name)   \
  __wfx_lua_loadscript(lua_state, name, wfx_lua_scripts)

static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;

int wfx_lua_require_module(lua_State *L) {
  wfx_module_lua_script_t *script;
  char                     scriptname[255];
  const char              *name;
  name = luaL_checkstring(L, 1);
  WFX_MODULE_LUA_SCRIPTS_EACH(script) {
    if(strcmp(script->name, name) == 0) {
      snprintf(scriptname, 255, "=module %s.lua", script->name);
      luaL_loadbuffer(L, script->script, strlen(script->script), scriptname);
      lua_ngxcall(L, 0, 1);
      return 1;
    }
  }
  ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "notfound");
  lua_pushnil(L);
  return 1;
}


ipc_t        *wfx_ipc = NULL;
lua_State    *wfx_Lua = NULL;
shmem_t      *wfx_shm = NULL;

void *wfx_shm_alloc(size_t sz) {
  return ngx_alloc(sz, ngx_cycle->log);
}
void *wfx_shm_calloc(size_t sz) {
  return ngx_calloc(sz, ngx_cycle->log);
}
void wfx_shm_free(void *ptr) {
  ngx_free(ptr);
}

static int wfx_init_bind_lua(lua_State *L) {
  //ruleset bindings
  wfx_lua_register(L, wfx_ruleset_bindings_set);
  lua_ngxcall(L, 0, 0);
  return 0;
}

static ngx_int_t ngx_http_wafflex_header_filter(ngx_http_request_t *r) {
  wfx_loc_conf_t       *cf = ngx_http_get_module_loc_conf(r, ngx_wafflex_module);
  wfx_ruleset_conf_t   *rcf;
  ERR("header filter");
  
  for(rcf = cf->ruleset; rcf != NULL; rcf = rcf->next) {
    ERR("do a ruleset for the headers");
  }
  
  return ngx_http_next_header_filter(r);
}
static ngx_int_t ngx_http_wafflex_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {
  wfx_loc_conf_t       *cf = ngx_http_get_module_loc_conf(r, ngx_wafflex_module);
  wfx_ruleset_conf_t   *rcf;
  ERR("body filter");
  for(rcf = cf->ruleset; rcf != NULL; rcf = rcf->next) {
    ERR("do a ruleset for the body");
  }
  
  return ngx_http_next_body_filter(r, in);
}

static int wfx_postinit_conf_attach_ruleset(lua_State *L) {
  wfx_ruleset_conf_t *rcf = lua_touserdata(L, 2);
  lua_getfield(L, 1, "__binding");
  rcf->ptr = lua_touserdata(L, -1);
  return 0;
}

ngx_int_t ngx_wafflex_init_lua(void) {  
  wfx_Lua = luaL_newstate();
  luaL_openlibs(wfx_Lua);
  
  wfx_lua_loadscript(wfx_Lua, init);
  wfx_lua_register(wfx_Lua, wfx_lua_require_module);
  wfx_lua_register(wfx_Lua, wfx_init_bind_lua);
  wfx_lua_register(wfx_Lua, wfx_postinit_conf_attach_ruleset);
  lua_ngxcall(wfx_Lua, 3, 0);
  
  return NGX_OK;
}
ngx_int_t ngx_wafflex_shutdown_lua(void) {
  lua_close(wfx_Lua);
  wfx_Lua = NULL;
  return NGX_OK;
}

ngx_int_t ngx_wafflex_inject_http_filters(void) {
  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = ngx_http_wafflex_header_filter;

  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ngx_http_wafflex_body_filter;
  
  ERR("injected filters");
  
  return NGX_OK;
}

void ngx_wafflex_ipc_alert_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data) {
  //do nothing -- for now
}
