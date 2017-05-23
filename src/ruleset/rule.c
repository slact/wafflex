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
