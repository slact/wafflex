#include <ngx_wafflex.h>
#include "common.h"
#include "phase.h"
#include "list.h"
#include "tracer.h"

#include <assert.h>

#define EXTRA_LISTS_SPACE 5

wfx_rc_t wfx_phase_eval(wfx_phase_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  int              len = self->len, i, start;
  wfx_rc_t         rc = WFX_OK;
  wfx_rule_list_t *list;
  
  if(!ruleset_common_reserve_read(ed, &self->rw)) {
    tracer_log_cstr(ed, "updating", "true");
    rc = WFX_DEFER;
    ctx->phase.gen = 0; // guaranteed(?) to differ from 'gen' value once update completes.
                        // don't want to read value as it's being updated
    ctx->phase.phase = ed->phase;
    return rc;
  }
  
  if(ctx->nocheck) {//from the top
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
        ruleset_common_release_read(&self->rw);
        return WFX_OK;
      case WFX_DEFER:
        ctx->phase.gen = self->gen;
        ctx->phase.phase = ed->phase;
        ruleset_common_release_read(&self->rw);
        return rc;
      default:
        ruleset_common_release_read(&self->rw);
        return rc;
    }
  }
  ruleset_common_release_read(&self->rw);
  return rc;
}

static int phase_create(lua_State *L) {
  int               lists_n, i;
  wfx_rule_list_t  *list;
  wfx_rule_list_t **lists_array;
  ERR("phase create");
  wfx_phase_t     *phase;
  
  lua_getfield(L, 1, "lists");
  lists_n = wfx_lua_len(L, -1); //number of rule lists for this phase
  lua_pop(L, -1);
  
  phase = ruleset_common_shm_alloc_init_item(wfx_phase_t, 0, L);
  lists_array = wfx_shm_alloc(sizeof(list) * (lists_n + EXTRA_LISTS_SPACE));
  
  if(phase == NULL || lists_array == NULL) {
    ERR("failed to create phase");
    return 0;
  }
  
  phase->len= lists_n;
  phase->max_len = lists_n + EXTRA_LISTS_SPACE;
  phase->gen = 0;
  
  phase->lists = lists_array;
  
  lua_getfield(L, 1, "lists");
  for(i=0; i< lists_n; i++) {
    lua_rawgeti(L, -1, i+1);
    lua_getfield(L, -1, "__binding");
    list = lua_touserdata(L, -1);
    lists_array[i] = list;
    lua_pop(L, 2);
  }
  lua_pop(L, 1);
  
  lua_pushlightuserdata (L, phase);
  return 1;
}

static int phase_update(lua_State *L) {
  // stack index 2: delta table
  // stack index 1: list (userdata)
  
  size_t               i, lists_n;
  wfx_phase_t         *phase = lua_touserdata(L, 1);
  wfx_rule_list_t    **new_lists;
  wfx_rule_list_t     *list;
  
  ERR("phase update");
  
  if(!ruleset_common_reserve_write(&phase->rw)) {
    ruleset_common_delay_update(L, &phase->rw, phase_update);
    return 0;
  }
  
  ruleset_common_update_item_name(L, &phase->name);
  
  lua_getfield(L, 2, "lists");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    lists_n = wfx_lua_len(L, -1); //number of rule lists for this phase
    if( phase->max_len < lists_n /* old phase isn't big enough */
     || lists_n * 2 < phase->max_len /* old phase is too big */) {
      new_lists = wfx_shm_alloc(sizeof(list) * (lists_n + EXTRA_LISTS_SPACE));
      if(new_lists == NULL) {
        ERR("couldn't allocate updated rule list");
        return 0;
      }
      wfx_shm_free(phase->lists);
      phase->lists = new_lists;
    }
    else {
      new_lists = phase->lists;
    }
    
    //update the C phase lists
    for(i=0; i< lists_n; i++) {
      lua_geti(L, -1, i+1);
      lua_getfield(L, -1, "__binding");
      list = lua_touserdata(L, -1);
      lua_pop(L, 2);
      new_lists[i]=list;
    }
    
    phase->len = lists_n;
    phase->max_len = lists_n + EXTRA_LISTS_SPACE;
    
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  phase->gen++;
  
  ruleset_common_release_write(&phase->rw);
  return 0;
}

static int phase_delete(lua_State *L) {
  wfx_phase_t     *phase = lua_touserdata(L, 1);
  wfx_shm_free(phase->lists);
  ruleset_common_shm_free_item(L, phase);
  return 0;
}

static wfx_binding_t wfx_phase_binding = {
  "phase",
  phase_create,
  phase_update,
  phase_delete
};

void wfx_phase_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_phase_binding);
}
