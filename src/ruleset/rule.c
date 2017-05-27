#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "rule.h"

wfx_rc_t wfx_rule_eval(wfx_rule_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  int                  i, n;
  wfx_action_t       **actions;
  wfx_rc_t             rc = WFX_OK;
  
  lua_geti(wfx_Lua, LUA_REGISTRYINDEX, self->luaref);
  lua_mm(wfx_Lua, -1);
  lua_pop(wfx_Lua, 1);
  
  if(self->condition->eval(self->condition, ed, &ctx->rule.condition_stack)) {
    actions = self->actions;
    n = self->actions_len;
  }
  else {
    actions = self->else_actions;
    n = self->else_actions_len;
  }
  
  for(i=0; i<n; i++) {
    rc = actions[i]->eval(actions[i], ed);
    switch(rc) {
      case WFX_OK:
        continue;
      default:
        return rc;
    }
  }
  
  return rc;
}

static int rule_create(lua_State *L) {
  ERR("rule create");
  wfx_rule_t     *rule;
  int i, then_actions = 0, else_actions = 0;
  
  lua_getfield(L, -1, "then");
  then_actions = wfx_lua_len(L, -1);
  lua_pop(L, 1);
  
  lua_getfield(L, -1, "else");
  else_actions = wfx_lua_len(L, -1);
  lua_pop(L, 1);
  
  rule = ruleset_common_shm_alloc_init_item(wfx_rule_t, (sizeof(wfx_action_t *) * (then_actions + else_actions)), L, name);
  if(rule == NULL) {
    return 0;
  }
  rule->gen = 0;
  
  lua_getfield(L, -1, "if");
  lua_getfield(L, -1, "__binding");
  rule->condition=lua_touserdata(L, -1);
  lua_pop(L, 2);
  
  rule->actions = (void *)&rule[1];
  rule->else_actions = &rule->actions[then_actions];
  rule->actions_len = then_actions;
  rule->else_actions_len = else_actions;
  
  lua_getfield(L, -1, "then");
  for(i=0; i<then_actions; i++) {
    lua_geti(L, -1, i+1);
    lua_getfield(L, -1, "__binding");
    rule->actions[i] = lua_touserdata(L, -1);
    lua_pop(L, 2);
  }
  lua_pop(L, 1);
  
  lua_getfield(L, -1, "else");
  for(i=0; i<else_actions; i++) {
    lua_geti(L, -1, i+1);
    lua_getfield(L, -1, "__binding");
    rule->else_actions[i] = lua_touserdata(L, -1);
    lua_pop(L, 2);
  }
  lua_pop(L, 1);
  
  lua_pushlightuserdata (L, rule);
  return 1;
}

static wfx_binding_t wfx_rule_binding = {
  "rule",
  rule_create,
  NULL,
  NULL,
  NULL
};

void wfx_rule_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_rule_binding);
}
