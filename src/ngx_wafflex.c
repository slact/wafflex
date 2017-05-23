#include <ngx_wafflex.h>
#include <util/ipc.h>
#include <util/shmem.h>
#include <ruleset/ruleset.h>
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


static ipc_t        *ipc = NULL;
static lua_State    *Lua = NULL;
shmem_t             *shm = NULL;

void *wfx_shm_alloc(size_t sz) {
  return ngx_alloc(sz, ngx_cycle->log);
}
void *wfx_shm_calloc(size_t sz) {
  return ngx_calloc(sz, ngx_cycle->log);
}
void wfx_shm_free(void *ptr) {
  ngx_free(ptr);
}

ngx_str_t *wfx_get_interpolated_string(const char *str) {
 //TODO
  return NULL; 
}

static int wfx_init_bind_lua(lua_State *L) {
  //ruleset bindings
  wfx_lua_register(L, wfx_ruleset_bindings_set);
  lua_ngxcall(L, 0, 0);
  return 0;
}

static ngx_int_t ngx_http_wafflex_header_filter(ngx_http_request_t *r) {
  return ngx_http_next_header_filter(r);
}
static ngx_int_t ngx_http_wafflex_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {
  return ngx_http_next_body_filter(r, in);
}


static ngx_int_t ngx_wafflex_init_postconfig(ngx_conf_t *cf) {  
  
  Lua = luaL_newstate();
  luaL_openlibs(Lua);
  
  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = ngx_http_wafflex_header_filter;

  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ngx_http_wafflex_body_filter;
  
  wfx_lua_loadscript(Lua, init);
  wfx_lua_register(Lua, wfx_lua_require_module);
  wfx_lua_register(Lua, wfx_init_bind_lua);
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
  lua_close(Lua);
  ipc_destroy(ipc);
}

static void ngx_wafflex_exit_master(ngx_cycle_t *cycle) {
  lua_close(Lua);
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
