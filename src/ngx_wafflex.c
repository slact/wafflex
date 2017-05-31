#include <ngx_wafflex.h>
#include <util/ipc.h>
#include <util/shmem.h>
#include <ruleset/ruleset.h>
#include <util/wfx_str.h>
#include <ruleset/condition.h>

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
  return shm_alloc(wfx_shm, sz, "wafflex item");
}
void *wfx_shm_calloc(size_t sz) {
  return shm_calloc(wfx_shm, sz, "wafflex item");
}
void wfx_shm_free(void *ptr) {
  shm_free(wfx_shm, ptr);
}

static int wfx_init_bind_lua(lua_State *L) {
  //ruleset bindings
  wfx_lua_register(L, wfx_ruleset_bindings_set);
  lua_ngxcall(L, 0, 0);
  return 0;
}

static void ngx_wfx_request_ctx_cleanup(wfx_request_ctx_t *ctx) {
  if(ctx) {
    condition_stack_clear(&ctx->rule.condition_stack);
  }
}

static ngx_int_t ngx_wafflex_request_handler(ngx_http_request_t *r) {
  wfx_loc_conf_t       *cf = ngx_http_get_module_loc_conf(r, ngx_wafflex_module);
  int                   start, i, n;
  wfx_ruleset_conf_t   *rcf;
  wfx_ruleset_t        *ruleset;
  wfx_evaldata_t        ed;
  wfx_rc_t              rc;
  wfx_request_ctx_t    *ctx, tmpctx;
  ngx_http_cleanup_t   *cln;
  if(!cf->rulesets) {
    return NGX_DECLINED;
  }
  
  ed.data.request = r;
  ed.type = WFX_EVAL_HTTP_REQUEST;
  ed.phase = WFX_PHASE_HTTP_REQUEST_HEADERS;
  
  tmpctx.nocheck = 1;
  ngx_memzero(&tmpctx.rule, sizeof(tmpctx.rule));
  
  n = cf->rulesets->nelts;
  rcf = cf->rulesets->elts;
  
  ctx = ngx_http_get_module_ctx(r, ngx_wafflex_module);
  if(ctx && !ctx->nocheck && ctx->ruleset.gen == rcf->ruleset[ctx->ruleset.i].gen) {
    //resume ruleset execution
    start = ctx->ruleset.i;
  }
  else {
    start = 0;
  }
  
  for(i=start; i<n; i++) {
    ruleset = &rcf->ruleset[i];
    rc = wfx_ruleset_eval(ruleset, &ed, ctx == NULL ? &tmpctx : ctx);
    //raise(SIGSTOP);
    switch(rc) {
      case WFX_OK:
        continue;
      case WFX_SKIP:
        return NGX_OK;
      case WFX_ACCEPT:
        return NGX_OK;
      case WFX_REJECT:
        return NGX_ABORT;
      case WFX_DEFER:
        if(ctx == NULL) {
          cln = ngx_http_cleanup_add(r, 0);
          ctx = ngx_palloc(r->pool, sizeof(*ctx));
          if (ctx == NULL || ctx == NULL)
            return NGX_ERROR;
          
          cln->handler = (ngx_http_cleanup_pt )ngx_wfx_request_ctx_cleanup;
          cln->data = ctx;
          
          *ctx = tmpctx;
          ctx->nocheck = 0;
          ngx_http_set_ctx(r, ctx, ngx_wafflex_module);
        }
        ctx->ruleset.i = i;
        return NGX_DONE;
      case WFX_ERROR:
        return NGX_ERROR;
    }
  }
  return NGX_OK;
}

static ngx_int_t ngx_http_wafflex_header_filter(ngx_http_request_t *r) {
  //wfx_loc_conf_t       *cf = ngx_http_get_module_loc_conf(r, ngx_wafflex_module);
  //wfx_ruleset_conf_t   *rcf;
  ERR("header filter");
  
  return ngx_http_next_header_filter(r);
}
static ngx_int_t ngx_http_wafflex_body_filter(ngx_http_request_t *r, ngx_chain_t *in) {
  //wfx_loc_conf_t       *cf = ngx_http_get_module_loc_conf(r, ngx_wafflex_module);
  //wfx_ruleset_conf_t   *rcf;
  ERR("body filter");
  
  return ngx_http_next_body_filter(r, in);
}

static int wfx_postinit_conf_attach_ruleset(lua_State *L) {
  wfx_ruleset_conf_t *rcf = lua_touserdata(L, 2);
  lua_getfield(L, 1, "__binding");
  rcf->ruleset = lua_touserdata(L, -1);
  return 0;
}

ngx_int_t ngx_wafflex_init_lua(int loader) {  
  wfx_Lua = luaL_newstate();
  luaL_openlibs(wfx_Lua);
  
  if(loader) {
    wfx_lua_loadscript(wfx_Lua, init);
    wfx_lua_register(wfx_Lua, wfx_lua_require_module);
    wfx_lua_register(wfx_Lua, wfx_init_bind_lua);
    wfx_lua_register(wfx_Lua, wfx_postinit_conf_attach_ruleset);
    lua_ngxcall(wfx_Lua, 3, 0);
  }
  else {
    wfx_lua_loadscript(wfx_Lua, tag);
  }
  
  return NGX_OK;
}
ngx_int_t ngx_wafflex_shutdown_lua(void) {
  lua_close(wfx_Lua);
  wfx_Lua = NULL;
  return NGX_OK;
}

ngx_int_t ngx_wafflex_init_runtime(int manager) {
  wfx_ruleset_init_runtime(wfx_Lua, manager);
  return NGX_OK;
}

ngx_int_t ngx_wafflex_setup_http_request_hooks(ngx_conf_t *cf) {
  
  ngx_http_handler_pt        *h;
  ngx_http_core_main_conf_t  *cmcf;

  cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

  h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
  if (h == NULL) {
    return NGX_ERROR;
  }

  *h = ngx_wafflex_request_handler;
  
  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = ngx_http_wafflex_header_filter;

  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ngx_http_wafflex_body_filter;
  
  return NGX_OK;
}

void ngx_wafflex_ipc_alert_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data) {
  //do nothing -- for now
}
