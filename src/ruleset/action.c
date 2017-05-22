#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "action.h"

static int action_create(lua_State *L) {
  ERR("action create");
  wfx_action_t     *action;
  action = ruleset_common_shm_alloc_init_item(wfx_action_t, 0, L, action);
  
  //TODO
  
  lua_pushlightuserdata (L, action);
  return 1;
}
static int action_replace(lua_State *L) {
  ERR("action replace");
  return 0;
}
static int action_update(lua_State *L) {
  ERR("action update");
  return 0;
}
static int action_delete(lua_State *L) {
  ERR("action delete");
  return 0;
}

int wfx_action_bind_lua(lua_State *L) {;
  lua_pushstring(L, "action");
  wfx_lua_register(L, action_create);
  wfx_lua_register(L, action_replace);
  wfx_lua_register(L, action_update);
  wfx_lua_register(L, action_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
