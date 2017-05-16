#include <ngx_wafflex.h>
#include <ipc.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "ngx_wafflex_parser_lua_scripts.h"

#include <assert.h>

//#define DEBUG_ON

#define LOAD_SCRIPTS_AS_NAMED_CHUNKS

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


#ifdef LOAD_SCRIPTS_AS_NAMED_CHUNKS
  #define ngx_wfx_parser_lua_loadscript(lua_state, name)                 \
          luaL_loadbuffer(lua_state, ngx_wfx_parser_lua_scripts.name, strlen(ngx_wfx_parser_lua_scripts.name), #name); \
          lua_call(lua_state, 0, LUA_MULTRET)
#else
  #define ngx_wfx_parser_lua_loadscript(lua_state, name)                 \
          luaL_dostring(lua_state, ngx_wfx_parser_lua_scripts.name)
#endif

ngx_int_t luaL_checklstring_as_ngx_str(lua_State *L, int n, ngx_str_t *str) {
  size_t         data_sz;
  const char    *data = luaL_checklstring(L, n, &data_sz);
  
  str->data = (u_char *)data;
  str->len = data_sz;
  return NGX_OK;
}

static ipc_t        *ipc = NULL;
static lua_State    *Lua = NULL;

static ngx_int_t ngx_wafflex_init_postconfig(ngx_conf_t *cf) {  
  Lua = luaL_newstate();
  luaL_openlibs(Lua);
  
  //load JSON parser
  ngx_wfx_parser_lua_loadscript(Lua, dkjson);
  
  //ruleset parser
  
  
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
