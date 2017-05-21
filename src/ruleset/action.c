#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "action.h"

static int action_create(lua_State *L) {
  ERR("action create");
  return 0;
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
  lua_pushcfunction(L, action_create);
  lua_pushcfunction(L, action_replace);
  lua_pushcfunction(L, action_update);
  lua_pushcfunction(L, action_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
