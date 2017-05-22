#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "rule.h"

static int rule_create(lua_State *L) {
  ERR("rule create");
  wfx_rule_t     *rule;
  rule = ruleset_common_shm_alloc_init_item(wfx_rule_t, 0, L, name);
  
  //TODO
  
  lua_pushlightuserdata (L, rule);
  return 1;
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
  wfx_lua_register(L, rule_create);
  wfx_lua_register(L, rule_replace);
  wfx_lua_register(L, rule_update);
  wfx_lua_register(L, rule_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
