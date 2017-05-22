#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "condition.h"

static int condition_create(lua_State *L) {
  ERR("condition create");
  wfx_condition_t     *condition;
  condition = ruleset_common_shm_alloc_init_item(wfx_condition_t, 0, L, condition);
  
  //TODO
  
  lua_pushlightuserdata (L, condition);
  return 1;
}
static int condition_replace(lua_State *L) {
  ERR("condition replace");
  return 0;
}
static int condition_update(lua_State *L) {
  ERR("condition update");
  return 0;
}
static int condition_delete(lua_State *L) {
  ERR("condition delete");
  return 0;
}

int wfx_condition_bind_lua(lua_State *L) {;
  lua_pushstring(L, "condition");
  wfx_lua_register(L, condition_create);
  wfx_lua_register(L, condition_replace);
  wfx_lua_register(L, condition_update);
  wfx_lua_register(L, condition_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
