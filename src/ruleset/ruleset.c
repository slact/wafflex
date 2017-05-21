#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "ruleset.h"
#include "list.h"
#include "rule.h"
#include "condition.h"
#include "action.h"
#include "limiter.h"


static int ruleset_create(lua_State *L) {
  ERR("ruleset create");
  return 0;
}
static int ruleset_replace(lua_State *L) {
  ERR("ruleset replace");
  return 0;
}
static int ruleset_update(lua_State *L) {
  ERR("ruleset update");
  return 0;
}
static int ruleset_delete(lua_State *L) {
  ERR("ruleset delete");
  return 0;
}

int wfx_ruleset_bind_lua(lua_State *L) {
  //rule bindings
  lua_pushcfunction(L, wfx_rule_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  //rule condition bindings
  lua_pushcfunction(L, wfx_condition_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  //rule action bindings
  lua_pushcfunction(L, wfx_action_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  //list bindings
  lua_pushcfunction(L, wfx_list_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  //limiter bindings
  lua_pushcfunction(L, wfx_limiter_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  //and now, at last, ruleset bindings
  lua_pushstring(L, "ruleset");
  lua_pushcfunction(L, ruleset_create);
  lua_pushcfunction(L, ruleset_replace);
  lua_pushcfunction(L, ruleset_update);
  lua_pushcfunction(L, ruleset_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
