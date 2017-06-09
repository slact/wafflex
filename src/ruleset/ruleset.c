#include <ngx_wafflex.h>
#include "common.h"
#include "ruleset.h"
#include "list.h"
#include "rule.h"
#include "condition.h"
#include "action.h"
#include "tag.h"
#include "limiter.h"
#include "phase.h"
#include "string.h"
#include "tracer.h"
static wfx_binding_t wfx_ruleset_binding;


wfx_rc_t wfx_ruleset_eval(wfx_ruleset_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  wfx_phase_t  *phase = self->phase[ed->phase];
  DBG("RULESET: #%i %s", ctx->ruleset.i, self->name);
  tracer_push(ed, WFX_PHASE, phase);
  wfx_rc_t      rc = wfx_phase_eval(phase, ed, ctx);
  tracer_pop(ed, WFX_PHASE, rc);
  if(rc == WFX_DEFER) {
    ctx->ruleset.gen = self->gen;
  }
  return rc;
}

static int ruleset_create(lua_State *L) {
  const char        *str;
  
  wfx_ruleset_t     *ruleset;
  wfx_phase_t       *phase = NULL;
  
  ruleset = ruleset_common_shm_alloc_init_item(wfx_ruleset_t, 0, L, name);
  if(ruleset == NULL) {
    ERR("failed to initialize ruleset: out of memory");
  }
  ruleset->gen = 0;
  
  lua_getfield(L, -1, "phases");
  lua_pushnil(L);  // first key 
  while (lua_next(L, -2) != 0) {
    lua_getfield(L, -1, "__binding");
    phase = lua_touserdata(L, -1);
    lua_pop(L,1);
    
    str = lua_tostring(L, -2);
    
    if(strcmp(str, "request") == 0) {
      ruleset->phase[WFX_PHASE_HTTP_REQUEST_HEADERS] = phase;
    }
    else {
      ERR("unknown phase %s", str);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  
  lua_pushlightuserdata (L, ruleset);
  return 1;
}

static int ruleset_delete(lua_State *L) {
  wfx_ruleset_t     *ruleset = lua_touserdata(L, 1);
  if (!ruleset) {
    lua_printstack(L);
    luaL_error(L, "expected ruleset __binding to be some value, bit got NULL");
    return 0;
  }
  
  luaL_unref(L, LUA_REGISTRYINDEX, ruleset->luaref);
  ruleset->luaref = LUA_NOREF;
  ruleset_common_shm_free(ruleset);
  return 0;
}

int wfx_ruleset_bindings_set(lua_State *L) {
  //string interpolation
  wfx_string_bindings_set(L);
  
  //rule bindings
  wfx_rule_bindings_set(L);
  
  //rule condition bindings
  wfx_condition_bindings_set(L);
  
  //rule action bindings
  wfx_action_bindings_set(L);
  //tag bindings
  wfx_tag_bindings_set(L);
  
  //list bindings
  wfx_list_bindings_set(L);
  
  //limiter bindings
  wfx_limiter_bindings_set(L);
  
  //phase bindings
  wfx_phase_bindings_set(L);
  
  //and now, at last, ruleset bindings
  wfx_lua_binding_set(L, &wfx_ruleset_binding);
  
  return 0;
}

void * __ruleset_common_shm_alloc_init_item_noname(lua_State *L, size_t item_sz, size_t data_sz, off_t luaref_offset) {
  int         tmpref;
  void       *ptr;
  int        *refptr;
  
  tmpref = wfx_lua_getref(L, 1);
  
  if((ptr = wfx_shm_calloc(item_sz + data_sz)) != NULL) {
    refptr = (int *)((char *)ptr + luaref_offset);
    *refptr = tmpref;
  }
  
  lua_pop(L, 1);
  return ptr;
}
  
void * __ruleset_common_shm_alloc_init_item(lua_State *L, size_t item_sz, size_t data_sz, char *str_key, off_t str_offset, off_t luaref_offset) {
  const char *tmpstr;
  size_t      tmpstrlen;
  int         tmpref;
  void       *ptr;
  char       *str;
  char      **strptr;
  int        *refptr;
  
  lua_pushvalue(L, 1); //lua-self
  tmpref = luaL_ref(L, LUA_REGISTRYINDEX);
  
  lua_pushvalue(L, 1); //lua-self
  lua_getfield(L, -1, str_key);
  tmpstr = lua_tolstring(L, -1, &tmpstrlen);
  
  if((ptr = wfx_shm_calloc(item_sz + data_sz + tmpstrlen+1)) != NULL) {
    str = (char *)ptr + item_sz + data_sz;
    strptr = (char **)((char *)ptr + str_offset);
    strcpy(str, tmpstr);
    *strptr = str;
    refptr = (int *)((char *)ptr + luaref_offset);
    *refptr = tmpref;
  }
  
  lua_pop(L, 2);
  return ptr;
}

void ruleset_common_shm_free(void *ptr) {
  wfx_shm_free(ptr);
}

static wfx_binding_t wfx_ruleset_binding = {
  "ruleset",
  ruleset_create,
  NULL,
  NULL,
  ruleset_delete
};

ngx_int_t wfx_ruleset_init_runtime(lua_State *L, int manager) {
  wfx_limiter_init_runtime(L, manager);
  wfx_tag_init_runtime(L, manager);
  wfx_tracer_init_runtime(L, manager);
  return NGX_OK;
}
