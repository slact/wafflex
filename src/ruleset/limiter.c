#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "limiter.h"

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

void wfx_limiter_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_limiter_binding);
}
