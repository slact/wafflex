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
