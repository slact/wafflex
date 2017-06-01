#include <ngx_wafflex.h>
#include "common.h"
#include "list.h"
#include "rule.h"

wfx_rc_t wfx_list_eval(wfx_rule_list_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx) {
  int                  start, i, len = self->len;
  wfx_rule_t         **rule = self->rules;
  wfx_rc_t             rc = WFX_OK;
  if(ctx->nocheck || ctx->list.gen != self->gen) {
    start = 0;
  }
  else {
    start = ctx->rule.i;
  }
  DBG("LIST: #%i %s", ctx->list.i, self->name);
  for(i=start; i < len; i++) {
    ctx->rule.i = i;
    rc = wfx_rule_eval(rule[i], ed, ctx);
    switch(rc) {
      case WFX_OK:
        continue;
      case WFX_SKIP: //next list
        return WFX_OK;
      case WFX_DEFER:
        ctx->list.gen = self->gen;
        return rc;
      default:
        return rc;
    }
  }
  return rc;
}

static int list_create(lua_State *L) {
  int                  i, rules_n;
  wfx_rule_list_t     *list;
  wfx_rule_t          *rule;
  
  ERR("list create");
  
  lua_getfield(L, -1, "rules");
  rules_n = wfx_lua_len(L, -1);
  lua_pop(L, 1);
  
  list = ruleset_common_shm_alloc_init_item(wfx_rule_list_t, (sizeof(rule)*(rules_n - 1)), L, name);
  
  list->len = rules_n;
  list->gen = 0;
  
  lua_getfield(L, -1, "rules");
  for(i=0; i<rules_n; i++) {
    lua_geti(L, -1, i+1);
    lua_getfield(L, -1, "__binding");
    rule = lua_touserdata(L, -1);
    lua_pop(L, 2);
    list->rules[i]=rule;
  }
  
  lua_pushlightuserdata(L, list);
  
  return 1;
}

static wfx_binding_t wfx_list_binding = {
  "list",
  list_create,
  NULL,
  NULL,
  NULL
};

void wfx_list_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_list_binding);
}
