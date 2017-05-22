#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "phase.h"
#include "list.h"

static int phase_create(lua_State *L) {
  int              len, i;
  wfx_rule_list_t *list;
  ERR("phase create");
  wfx_phase_t     *phase;
  
  lua_getfield(L, 1, "lists");
  len = luaL_len(L, -1); //number of rule lists for this phase
  lua_printstack(L);
  phase = ruleset_common_shm_alloc_init_item(wfx_phase_t, sizeof(wfx_rule_list_t) * (len - 1), L, name);
  lua_printstack(L);
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

static int phase_replace(lua_State *L) {
  ERR("phase replace");
  return 0;
}
static int phase_update(lua_State *L) {
  ERR("phase update");
  return 0;
}
static int phase_delete(lua_State *L) {
  ERR("phase delete");
  return 0;
}

int wfx_phase_bind_lua(lua_State *L) {;
  lua_pushstring(L, "phase");
  wfx_lua_register(L, phase_create);
  wfx_lua_register(L, phase_replace);
  wfx_lua_register(L, phase_update);
  wfx_lua_register(L, phase_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
