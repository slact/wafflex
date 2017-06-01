#include <ngx_wafflex.h>
#include "common.h"
#include "rule.h"
#include "condition.h"

static wfx_rc_t wfx_rule_actions_eval(wfx_rule_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  int              i, start, n;
  wfx_rc_t         rc = WFX_OK;
  wfx_action_t   **actions;
  if(ctx->rule.condition_true) {
    actions = self->actions;
    n = self->actions_len;
  }
  else {
    actions = self->else_actions;
    n = self->else_actions_len;
  }
  start = ctx->nocheck ? 0 : ctx->rule.action.i;
  for(i=start; i<n; i++) {
    rc = actions[i]->eval(actions[i], ed);
    switch(rc) {
      case WFX_OK:
        continue;
      case WFX_DEFER:
        ctx->rule.action.i = i;
        ctx->rule.action.data = NULL;
        return rc;
      default:
        return rc;
    }
  }
  return rc;
}

wfx_rc_t wfx_rule_eval(wfx_rule_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  wfx_condition_rc_t        cond_rc;
  wfx_rc_t                  rc;
  if(!ctx->nocheck && ctx->rule.gen != self->gen) {
    ERR("rule was changed, restart it from the top");
    condition_stack_clear(&ctx->rule.condition_stack);
    ngx_memzero(&ctx->rule, sizeof(ctx->rule));
  }
  
  if(ctx->nocheck || !ctx->rule.condition_done) {
    cond_rc = self->condition->eval(self->condition, ed, &ctx->rule.condition_stack);
    switch(cond_rc) {
      case WFX_COND_TRUE:
      case WFX_COND_FALSE:
        ctx->rule.condition_done = 1;
        ctx->rule.condition_true = WFX_COND_TRUE ? 1 : 0;
        rc = wfx_rule_actions_eval(self, ed, ctx);
        return rc;
      case WFX_COND_ERROR:
        return WFX_ERROR;
      case WFX_COND_DEFER:
        ctx->rule.gen = self->gen;
        return WFX_DEFER;
    }
  }
  else if(ctx->rule.condition_done) {
    return wfx_rule_actions_eval(self, ed, ctx);
  }
  ERR("not supposed to happen");
  raise(SIGABRT);
  return WFX_ERROR;
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
