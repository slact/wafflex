#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "limiter.h"
#include "condition.h"
#include <util/wfx_str.h>

#define DEFAULT_SYNC_STEPS 4

static int limiter_create(lua_State *L) {
  //expecting a limiter table at top of stack
  wfx_limiter_t     *limiter;
  limiter = ruleset_common_shm_alloc_init_item(wfx_limiter_t, 0, L, name);
  if(limiter == NULL) {
    ERR("failed to initialize limiter: out of memory");
  }
  
  lua_getfield(L, -1, "limit");
  limiter->limit = lua_tonumber(L, -1);
  lua_pop(L,1);
  
  lua_getfield(L, -1, "interval");
  limiter->interval = lua_tonumber(L, -1);
  lua_pop(L,1);
  
  lua_getfield(L, -1, "burst");
  if lua_isnil(L, -1) {
    limiter->burst.limiter = NULL;
    limiter->burst.expire = 0;
  }
  else {
    lua_getfield(L, -1, "__binding");
    limiter->burst.limiter = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    lua_getfield(L, -2, "burst_expire");
    limiter->burst.expire = lua_isnil(L, -1) ? -1 : lua_tonumber(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
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
  wfx_str_t       *key;
  int              increment;
} limit_condition_data_t;
static wfx_condition_rc_t condition_limit_break_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  //todo
  return WFX_COND_TRUE;
}
static int condition_limit_break_create(lua_State *L) {
  limit_condition_data_t *data;
  wfx_condition_t *limit_break = condition_create(L, sizeof(*data), condition_limit_break_eval);
  limit_break->data.type = WFX_DATA_PTR;
  limit_break->data.count = 1;
  limit_break->data.data.ptr = &limit_break[1];
  data = limit_break->data.data.ptr;
  
  lua_getfield(L, -2, "data");
  
  lua_getfield(L, -1, "limiter");
  lua_getfield(L, -1, "__binding");
  data->limiter = lua_touserdata(L, -1);
  lua_pop(L, 2);
  
  lua_getfield(L, -1, "key");
  data->key = wfx_lua_get_str_binding(L, -1);
  lua_pop(L, 1);
  
  lua_getfield(L, -1, "increment");
  data->increment = lua_tonumber(L, -1);
  lua_pop(L, 1);
  
  lua_pop(L, 1); //pop data
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
