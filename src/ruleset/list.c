#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "list.h"

static int list_create(lua_State *L) {
  ERR("list create");
  return 0;
}
static int list_replace(lua_State *L) {
  ERR("list replace");
  return 0;
}
static int list_update(lua_State *L) {
  ERR("list update");
  return 0;
}
static int list_delete(lua_State *L) {
  ERR("list delete");
  return 0;
}

int wfx_list_bind_lua(lua_State *L) {;
  lua_pushstring(L, "list");
  lua_pushcfunction(L, list_create);
  lua_pushcfunction(L, list_replace);
  lua_pushcfunction(L, list_update);
  lua_pushcfunction(L, list_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
