#include <ngx_wafflex.h>
#include "common.h"
#include "rule.h"
#include "condition.h"
#include "tracer.h"

#include <assert.h>

static wfx_rc_t wfx_rule_actions_eval(wfx_rule_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  int              i, start, n;
  wfx_rc_t         rc = WFX_OK;
  wfx_action_t   **actions;
  wfx_action_t    *action;
  if(ctx->rule.condition_true) {
    actions = &self->all_actions[0];
    n = self->then_actions_count;
  }
  else {
    actions = &self->all_actions[self->then_actions_count];
    n = self->else_actions_count;
  }
  start = ctx->nocheck ? 0 : ctx->rule.action.i;
  for(i=start; i<n; i++) {
    action = actions[i];
    tracer_push(ed, WFX_ACTION, action);
    rc = actions[i]->eval(action, ed);
    tracer_pop(ed, WFX_ACTION, rc);
    switch(rc) {
      case WFX_OK:
        ctx->rule.action.data = NULL;
        continue;
      case WFX_DEFER:
        ctx->rule.gen = self->gen;
        ctx->rule.action.i = i;
        ctx->rule.action.data = NULL;
        return rc;
      default:
        return rc;
    }
  }
  ctx->rule.condition_done = 0;
  ctx->rule.condition_true = 0;
  return rc;
}

wfx_rc_t wfx_rule_eval(wfx_rule_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  wfx_rc_t                  rc;
  wfx_condition_rc_t        cond_rc;
  DBG("RULE: #%i %s", ctx->rule.i, self->name);
  
  if(!ruleset_common_reserve_read(ed, &self->rw)) {
    tracer_log_cstr(ed, "updating", "true");
    return WFX_DEFER;
  }
  
  if(self->disabled) {
    tracer_log_cstr(ed, "disabled", "true");
    rc = WFX_OK;
  }
  else {
    if(!ctx->nocheck && ctx->rule.gen != self->gen) {
      ERR("rule was changed, restart it from the top");
      condition_stack_clear(&ctx->rule.condition_stack);
      ngx_memzero(&ctx->rule, sizeof(ctx->rule));
      tracer_unwind(ed, WFX_RULE, "rule has changed");
    }
    if(ctx->nocheck || !ctx->rule.condition_done) {
      tracer_push(ed, WFX_CONDITION, self->condition);
      cond_rc = self->condition->eval(self->condition, ed, &ctx->rule.condition_stack);
      tracer_pop(ed, WFX_CONDITION, cond_rc);
      switch(cond_rc) {
        case WFX_COND_TRUE:
        case WFX_COND_FALSE:
          ctx->rule.condition_done = 1;
          ctx->rule.condition_true = cond_rc == WFX_COND_TRUE ? 1 : 0;
          rc = wfx_rule_actions_eval(self, ed, ctx);
          break;
        case WFX_COND_ERROR:
          rc = WFX_ERROR;
          break;
        case WFX_COND_DEFER:
          rc = WFX_DEFER;
          break;
      }
    }
    else if(ctx->rule.condition_done) {
      rc = wfx_rule_actions_eval(self, ed, ctx);
    }
    else {
      ERR("not supposed to happen");
      raise(SIGABRT);
      rc = WFX_ERROR;
    }
  }
  ruleset_common_release_read(&self->rw);
  return rc;
}

static int rule_create(lua_State *L) {
  ERR("rule create");
  wfx_rule_t     *rule;
  wfx_action_t   *(*actions);
  int i, n=0, then_actions = 0, else_actions = 0;
  
  lua_getfield(L, -1, "then");
  then_actions = wfx_lua_len(L, -1);
  lua_pop(L, 1);
  
  lua_getfield(L, -1, "else");
  else_actions = wfx_lua_len(L, -1);
  lua_pop(L, 1);
  
  rule = ruleset_common_shm_alloc_init_item(wfx_rule_t, 0, L);
  actions = wfx_shm_alloc(sizeof(*actions) * (then_actions + else_actions));
  
  lua_getfield(L, -1, "disabled");
  rule->disabled = lua_toboolean(L, -1);
  lua_pop(L, 1);
  
  if(rule == NULL || actions == NULL) {
    return 0;
  }
  rule->all_actions = actions;
  rule->gen = 0;
  
  lua_getfield(L, -1, "if");
  lua_getfield(L, -1, "__binding");
  rule->condition=lua_touserdata(L, -1);
  lua_pop(L, 2);
  
  rule->then_actions_count = then_actions;
  rule->else_actions_count = else_actions;
  
  lua_getfield(L, -1, "then");
  for(i=0; i<then_actions; i++) {
    lua_geti(L, -1, i+1);
    lua_getfield(L, -1, "__binding");
    rule->all_actions[n++] = lua_touserdata(L, -1);
    lua_pop(L, 2);
  }
  lua_pop(L, 1);
  
  lua_getfield(L, -1, "else");
  for(i=0; i<else_actions; i++) {
    lua_geti(L, -1, i+1);
    lua_getfield(L, -1, "__binding");
    rule->all_actions[n++] = lua_touserdata(L, -1);
    lua_pop(L, 2);
  }
  lua_pop(L, 1);
  
  lua_pushlightuserdata (L, rule);
  return 1;
}

static int rule_update(lua_State *L) {
  // stack index 2: delta table
  // stack index 1: rule (userdata)
  
  wfx_rule_t    *rule = lua_touserdata(L, 1);
  
  if(!ruleset_common_reserve_write(&rule->rw)) {
    ruleset_common_delay_update(L, &rule->rw, rule_update);
    return 0;
  }
  
  int            i, n = 0, update_actions = 0, then_actions = 0, else_actions = 0;
  ERR("rule update");
  
  ruleset_common_update_item_name(L, &rule->name);
  
  lua_getfield(L, 2, "disabled");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    rule->disabled = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  lua_getfield(L, 2, "if");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    lua_getfield(L, -1, "__binding");
    rule->condition=lua_touserdata(L, -1);
    lua_pop(L, 2);
  }
  lua_pop(L, 1);
  
  lua_getfield(L, 2, "then");
  if(!lua_isnil(L, -1)) {
    update_actions = 1;
    lua_getfield(L, -1, "new");
    then_actions = wfx_lua_len(L, -1);
    lua_pop(L, 2);
  }
  lua_pop(L, 1);
  
  lua_getfield(L, 2, "else");
  if(!lua_isnil(L, -1)) {
    update_actions = 1;
    lua_getfield(L, -1, "new");
    else_actions = wfx_lua_len(L, -1);
    lua_pop(L, 2);
  }
  lua_pop(L, 1);
  
  if(update_actions) {
    wfx_shm_free(rule->all_actions);
    rule->all_actions = wfx_shm_alloc(sizeof(*(rule->all_actions)) * (then_actions + else_actions));
    rule->then_actions_count = then_actions;
    rule->else_actions_count = else_actions;
    if(then_actions > 0) {
      lua_getfield(L, 2, "then");
      lua_getfield(L, -1, "new");
      for(i=0; i<then_actions; i++) {
        lua_geti(L, -1, i+1);
        lua_getfield(L, -1, "__binding");
        rule->all_actions[n++] = lua_touserdata(L, -1);
        lua_pop(L, 2);
      }
      lua_pop(L, 2);
    }
    
    if(then_actions > 0) {
      lua_getfield(L, 2, "else");
      lua_getfield(L, -1, "new");
      for(i=0; i<else_actions; i++) {
        lua_geti(L, -1, i+1);
        lua_getfield(L, -1, "__binding");
        rule->all_actions[n++] = lua_touserdata(L, -1);
        lua_pop(L, 2);
      }
      lua_pop(L, 2);
    }
  }
  rule->gen++;
  
  ruleset_common_release_write(&rule->rw);
  return 0;
}

static int rule_delete(lua_State *L) {
  wfx_rule_t    *rule = lua_touserdata(L, 1);
  wfx_action_t **actions;
  if (!rule) {
    lua_printstack(L);
    luaL_error(L, "expected rule __binding to be some value, bit got NULL");
    return 0;
  }
  
  actions = rule->all_actions;
  ruleset_common_shm_free_item(L, rule);
  wfx_shm_free(actions);
  return 0;
}

static wfx_binding_t wfx_rule_binding = {
  "rule",
  rule_create,
  rule_update,
  rule_delete
};

void wfx_rule_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_rule_binding);
}
