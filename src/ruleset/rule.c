#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "rule.h"

static int rule_create(lua_State *L) {
  ERR("rule create");
  return 0;
}
static int rule_replace(lua_State *L) {
  ERR("rule replace");
  return 0;
}
static int rule_update(lua_State *L) {
  ERR("rule update");
  return 0;
}
static int rule_delete(lua_State *L) {
  ERR("rule delete");
  return 0;
}

int wfx_rule_bind_lua(lua_State *L) {;
  lua_pushstring(L, "rule");
  lua_pushcfunction(L, rule_create);
  lua_pushcfunction(L, rule_replace);
  lua_pushcfunction(L, rule_update);
  lua_pushcfunction(L, rule_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
