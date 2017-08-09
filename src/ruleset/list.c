#include <ngx_wafflex.h>
#include "common.h"
#include "list.h"
#include "rule.h"
#include "tracer.h"

#include <assert.h>

#define EXTRA_RULES_SPACE 5

wfx_rc_t wfx_list_eval(wfx_rule_list_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  int                  start, i, len = self->len;
  wfx_rule_t         **rule = self->rules;
  wfx_rc_t             rc = WFX_OK;
  
  if(!ruleset_common_reserve_read(ed, &self->rw)) {
    tracer_log_cstr(ed, "updating", "true");
    rc = WFX_DEFER;
    ctx->list.gen = 0; // guaranteed(?) to differ from 'gen' value once update completes.
                       // don't want to read value as it's being updated
    return rc;
  }
  
  if(self->disabled) {
    tracer_log_cstr(ed, "disabled", "true");
    ruleset_common_release_read(ed, &self->rw);
    return WFX_OK;
  }
  
  if(ctx->nocheck) {
    start = 0;
  }
  else if(ctx->list.gen != self->gen) {
    start = 0;
    tracer_unwind(ed, WFX_LIST, "list has changed");
  }
  else {
    start = ctx->rule.i;
  }
  DBG("LIST: #%i %s", ctx->list.i, self->name);
  for(i=start; i < len; i++) {
    ctx->rule.i = i;
    tracer_push(ed, WFX_RULE, rule[i]);
    rc = wfx_rule_eval(rule[i], ed, ctx);
    tracer_pop(ed, WFX_RULE, rc);
    switch(rc) {
      case WFX_OK:
        continue;
      case WFX_SKIP: //next list
        ruleset_common_release_read(ed, &self->rw);
        return WFX_OK;
      case WFX_DEFER:
        ctx->list.gen = self->gen;
        ruleset_common_release_read(ed, &self->rw);
        return rc;
      default:
        ruleset_common_release_read(ed, &self->rw);
        return rc;
    }
  }
  ctx->rule.i = 0;
  ruleset_common_release_read(ed, &self->rw);
  return rc;
}

static int list_create(lua_State *L) {
  int                  i, rules_n;
  wfx_rule_list_t     *list;
  wfx_rule_t          *rule;
  wfx_rule_t         **rules_array;
  
  ERR("list create");
  
  lua_getfield(L, -1, "rules");
  rules_n = wfx_lua_len(L, -1);
  lua_pop(L, 1);
  
  list = ruleset_common_shm_alloc_init_item(wfx_rule_list_t, 0, L);
  rules_array = wfx_shm_alloc((sizeof(rule)*(rules_n + EXTRA_RULES_SPACE)));
  
  lua_getfield(L, -1, "disabled");
  list->disabled = lua_toboolean(L, -1);
  lua_pop(L, 1);
  
  if(list == NULL || rules_array == NULL) {
    ERR("failed to create list");
    return 0;
  }
  
  list->len = rules_n;
  list->max_len = rules_n + EXTRA_RULES_SPACE;
  list->gen = 0;
  
  list->rules = rules_array;
  
  lua_getfield(L, -1, "rules");
  for(i=0; i<rules_n; i++) {
    lua_geti(L, -1, i+1);
    lua_getfield(L, -1, "__binding");
    rule = lua_touserdata(L, -1);
    rules_array[i]=rule;
    lua_pop(L, 2);
  }
  
  lua_pushlightuserdata(L, list);
  
  return 1;
}

static int list_update(lua_State *L) {
  // stack index 2: delta table
  // stack index 1: list (userdata)
  
  size_t               i, rules_n;
  wfx_rule_list_t     *list = lua_touserdata(L, 1);
  wfx_rule_t         **new_rules;
  wfx_rule_t          *rule;
  
  ERR("list update");
  
  assert(list->rw.reading == 0);
  assert(list->rw.writing == 1);
  list->rw.writing = 2;
  
  ruleset_common_update_item_name(L, &list->name);
  
  lua_getfield(L, 2, "disabled");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    list->disabled = lua_toboolean(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  lua_getfield(L, 2, "rules");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    rules_n = wfx_lua_len(L, -1);
    if( list->max_len < rules_n /* old list isn't big enough */
     || rules_n * 2 < list->max_len /* old list is too big */) {
      new_rules = wfx_shm_alloc(sizeof(rule) * (rules_n + EXTRA_RULES_SPACE));
      if(new_rules == NULL) {
        ERR("couldn't allocate updated rule list");
        return 0;
      }
      wfx_shm_free(list->rules);
      list->rules = new_rules;
    }
    else {
      new_rules = list->rules;
    }
    
    //update the C rules list
    for(i=0; i<rules_n; i++) {
      lua_geti(L, -1, i+1);
      lua_getfield(L, -1, "__binding");
      rule = lua_touserdata(L, -1);
      lua_pop(L, 2);
      new_rules[i]=rule;
    }
    list->len = rules_n;
    list->max_len = rules_n + EXTRA_RULES_SPACE;
    
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  
  list->gen++;
  
  list->rw.writing = 0;
  
  return 0;
}

static int list_delete(lua_State *L) {
  wfx_rule_list_t     *list = lua_touserdata(L, 1);
  wfx_shm_free(list->rules);
  ruleset_common_shm_free_item(L, list);
  return 0;
}

static wfx_binding_t wfx_list_binding = {
  "list",
  list_create,
  list_update,
  list_delete
};

void wfx_list_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_list_binding);
}
