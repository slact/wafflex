#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "condition.h"

static int condition_create(lua_State *L) {
  ERR("condition create");
  return 0;
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
  lua_pushcfunction(L, condition_create);
  lua_pushcfunction(L, condition_replace);
  lua_pushcfunction(L, condition_update);
  lua_pushcfunction(L, condition_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
