#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "phase.h"
#include "list.h"

wfx_rc_t wfx_phase_eval(wfx_phase_t *self, wfx_evaldata_t *ed) {
  int        len, i;
  wfx_rc_t   rc;
  len = self->len;
  
  for(i=0; i<len; i++) {
    rc = wfx_list_eval(self->lists[i], ed);
    switch(rc) {
      case WFX_OK:
        continue;
      case WFX_SKIP:
        return WFX_OK;
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

static wfx_binding_t wfx_phase_binding = {
  "phase",
  phase_create,
  NULL,
  NULL,
  NULL
};

void wfx_phase_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_phase_binding);
}
