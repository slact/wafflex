#include <ngx_wafflex.h>
#include "common.h"
#include "phase.h"
#include "list.h"
#include "tracer.h"

#include <assert.h>

wfx_rc_t wfx_phase_eval(wfx_phase_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  int              len = self->len, i, start;
  wfx_rc_t         rc = WFX_OK;
  wfx_rule_list_t *list;
  if( ctx->nocheck) {//from the top
    start = 0;
  }
  else if(self->gen != ctx->phase.gen) { // phase has changed, start over
    start = 0;
    tracer_unwind(ed, WFX_PHASE, "phase has changed");
  }
  else { //resuming
    assert(ctx->list.i < len);
    start = ctx->list.i;
  }
  DBG("PHASE: %s", self->name);
  for(i=start; i<len; i++) {
    ctx->list.i = i;
    list = self->lists[i];
    tracer_push(ed, WFX_LIST, list);
    rc = wfx_list_eval(list, ed, ctx);
    tracer_pop(ed, WFX_LIST, rc);
    switch(rc) {
      case WFX_OK:
        continue;
      case WFX_SKIP:
        return WFX_OK;
      case WFX_DEFER:
        ctx->phase.gen = self->gen;
        ctx->phase.phase = ed->phase;
        return rc;
      default:
        return rc;
    }
  }
  return rc;
}

static int phase_create(lua_State *L) {
  int               len, i;
  wfx_rule_list_t  *list;
  ERR("phase create");
  wfx_phase_t     *phase;
  
  lua_getfield(L, 1, "lists");
  len = wfx_lua_len(L, -1); //number of rule lists for this phase
  phase = ruleset_common_shm_alloc_init_item(wfx_phase_t, sizeof(list) * (len - 1), L, name);
  phase->len=len;
  phase->gen = 0;
  for(i=0; i<len; i++) {
    lua_rawgeti(L, -1, i+1);
    lua_getfield(L, -1, "__binding");
    list = lua_touserdata(L, -1);
    phase->lists[i] = list;
    lua_pop(L, 2);
  }
  
  lua_pushlightuserdata (L, phase);
  return 1;
}

static int phase_delete(lua_State *L) {
  wfx_phase_t     *phase = lua_touserdata(L, 1);
  if (!phase) {
    lua_printstack(L);
    luaL_error(L, "expected phase __binding to be some value, bit got NULL");
    return 0;
  }
  
  luaL_unref(L, LUA_REGISTRYINDEX, phase->luaref);
  phase->luaref = LUA_NOREF;
  ruleset_common_shm_free(phase);
  return 0;
}

static wfx_binding_t wfx_phase_binding = {
  "phase",
  phase_create,
  NULL,
  NULL,
  phase_delete
};

void wfx_phase_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_phase_binding);
}
