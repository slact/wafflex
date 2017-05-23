#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "limiter.h"
#include "condition.h"

static int limiter_create(lua_State *L) {
  //expecting a limiter table at top of stack
  wfx_limiter_t     *limiter;
  limiter = ruleset_common_shm_alloc_init_item(wfx_limiter_t, 0, L, name);
  if(limiter == NULL) {
    ERR("failed to initialize limiter: out of memory");
  }
  
  //todo: initialize burst limiter reference
  
  lua_pushlightuserdata(L, limiter);
  return 1;
}

static wfx_binding_t wfx_limiter_binding = {
  "limiter",
  limiter_create,
  NULL,
  NULL,
  NULL
};

//limiter conditions
typedef struct {
  wfx_limiter_t   *limiter;
  ngx_str_t        key;
  int              increment;
} limit_condition_data_t;
static int condition_limit_break_eval(wfx_condition_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  return 1;
}
static int condition_limit_break_create(lua_State *L) {
  limit_condition_data_t *data;
  wfx_condition_t *limit_break = condition_create(L, sizeof(*data), condition_limit_break_eval);
  data = limit_break->data;
  
  return 1;
}
static wfx_condition_type_t limit_break = {
  "limit-break",
  condition_limit_break_create,
  NULL
};





void wfx_limiter_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_limiter_binding);
  wfx_condition_binding_add(L, &limit_break);
  
  
  
}
