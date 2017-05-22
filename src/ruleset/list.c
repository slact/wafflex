#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "list.h"

static int list_create(lua_State *L) {
  ERR("list create");
  wfx_rule_list_t     *list;
  
  
  list = ruleset_common_shm_alloc_init_item(wfx_rule_list_t, 0, L, name);
  
  //TODO
  
  lua_pushlightuserdata (L, list);
  return 1;
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
  wfx_lua_register(L, list_create);
  wfx_lua_register(L, list_replace);
  wfx_lua_register(L, list_update);
  wfx_lua_register(L, list_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
