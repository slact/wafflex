#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "limiter.h"

static int limiter_create(lua_State *L) {
  ERR("limiter create");
  return 0;
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
  lua_pushcfunction(L, limiter_create);
  lua_pushcfunction(L, limiter_replace);
  lua_pushcfunction(L, limiter_update);
  lua_pushcfunction(L, limiter_delete);
  lua_ngxcall(L,5,0);
  return 0;
}
