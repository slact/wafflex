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

#include <assert.h>

static wfx_binding_t wfx_ruleset_binding;


wfx_rc_t wfx_ruleset_eval(wfx_ruleset_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  wfx_phase_t  *phase = self->phase[ed->phase];
  wfx_rc_t      rc;
  DBG("RULESET: #%i %s", ctx->ruleset.i, self->name);
  tracer_push(ed, WFX_PHASE, phase);
  if(ruleset_common_reserve_read(ed, &self->rw)) {
    if(!self->disabled) {
      rc = wfx_phase_eval(phase, ed, ctx);
    }
    else {
      tracer_log_cstr(ed, "disabled", "true");
      rc = WFX_OK;
    }
  }
  else {
    tracer_log_cstr(ed, "updating", "true");
    rc = WFX_DEFER;
  }
  if(rc == WFX_DEFER) {
    ctx->ruleset.gen = self->gen;
  }
  ruleset_common_release_read(&self->rw);
  tracer_pop(ed, WFX_PHASE, rc);
  return rc;
}

static int ruleset_create(lua_State *L) {
  const char        *str;
  
  wfx_ruleset_t     *ruleset;
  wfx_phase_t       *phase = NULL;
  
  ruleset = ruleset_common_shm_alloc_init_item(wfx_ruleset_t, 0, L);
  if(ruleset == NULL) {
    ERR("failed to initialize ruleset: out of memory");
  }
  ruleset->gen = 0;
  
  lua_getfield(L, -1, "disabled");
  ruleset->disabled = lua_toboolean(L, -1);
  lua_pop(L, 1);
  
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

static int ruleset_update(lua_State *L) {
  // stack index 2: delta table
  // stack index 1: ruleset (userdata)
  
  wfx_ruleset_t *ruleset = lua_touserdata(L, 1);
  
  if(!ruleset_common_reserve_write(&ruleset->rw)) {
    ruleset_common_delay_update(L, &ruleset->rw, ruleset_update);
    return 0;
  }
  
  ruleset_common_update_item_name(L, &ruleset->name);
  
  //TODO: do we need to update phases possibly? Not sure...
  
  lua_getfield(L, 2, "disabled");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    ruleset->disabled = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  ruleset->gen++;
  
  ruleset_common_release_write(&ruleset->rw);
  return 0;
}

static int ruleset_delete(lua_State *L) {
  wfx_ruleset_t     *ruleset = lua_touserdata(L, 1);
  if (!ruleset) {
    lua_printstack(L);
    luaL_error(L, "expected ruleset __binding to be some value, bit got NULL");
    return 0;
  }
  
  ruleset_common_shm_free_item(L, ruleset);
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

int ruleset_common_update_item_name(lua_State *L, char **nameptr) {
  int          updated = 0;
  size_t       tmpstrlen;
  const char  *tmpstr;
  lua_getfield(L, 2, "name");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    tmpstr = lua_tolstring(L, -1, &tmpstrlen);
    
    wfx_shm_free(*nameptr);
    *nameptr = wfx_shm_alloc(tmpstrlen + 1);
    if(*nameptr != NULL) {
      strcpy(*nameptr, tmpstr);
      *nameptr[tmpstrlen]='\0';
      updated = 1;
    }
    else {
      *nameptr = "???";
      ERR("unable to allocathe shared memory for item name");
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  return updated;
}

void * __ruleset_common_shm_alloc_init_item_noname(lua_State *L, size_t item_sz, size_t data_sz, off_t luaref_offset) {
  int         tmpref;
  void       *ptr;
  int        *refptr;
  
  tmpref = wfx_lua_ref(L, 1);
  
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
  
  tmpref = wfx_lua_ref(L, 1);
  
  lua_pushvalue(L, 1); //lua-self
  lua_getfield(L, -1, str_key);
  tmpstr = lua_tolstring(L, -1, &tmpstrlen);
  
  ptr = wfx_shm_calloc(item_sz + data_sz);
  str = wfx_shm_alloc(tmpstrlen + 1);
  
  if(ptr == NULL || str == NULL) {
    ERR("unable to allocate shme ruleset item");
    return NULL;
  }

  strptr = (char **)((char *)ptr + str_offset);
  strcpy(str, tmpstr);
  str[tmpstrlen]='\0';
  
  *strptr = str;
  refptr = (int *)((char *)ptr + luaref_offset);
  *refptr = tmpref;
  
  lua_pop(L, 2);
  return ptr;
}

void __ruleset_common_shm_free_item(lua_State *L, void *ptr, char *name_str) {
  if(name_str != NULL) {
    wfx_shm_free(name_str);
  }
  wfx_shm_free(ptr);
}

static wfx_binding_t wfx_ruleset_binding = {
  "ruleset",
  ruleset_create,
  ruleset_update,
  ruleset_delete
};

ngx_int_t wfx_ruleset_init_runtime(lua_State *L, int manager) {
  wfx_limiter_init_runtime(L, manager);
  wfx_tag_init_runtime(L, manager);
  wfx_tracer_init_runtime(L, manager);
  
  wfx_readwrite_init_runtime(L, manager);
  
  return NGX_OK;
}
