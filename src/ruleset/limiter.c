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

static int limiter_replace(lua_State *L) {
  ERR("limiter replace");
  return 0;
}
static int limiter_update(lua_State *L) {
  ERR("limiter update");
  return 0;
}
static int limiter_delete(lua_State *L) {
  ERR("limiter delete");
  return 0;
}

int wfx_limiter_bind_lua(lua_State *L) {;
  lua_pushstring(L, "limiter");
  wfx_lua_register(L, limiter_create);
  wfx_lua_register(L, limiter_replace);
  wfx_lua_register(L, limiter_update);
  wfx_lua_register(L, limiter_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
