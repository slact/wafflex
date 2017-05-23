#ifndef NGX_WAFFLEX_TYPES_H
#define NGX_WAFFLEX_TYPES_H
#include <lua.h>

typedef struct {
  const char           *name;
  lua_CFunction         create;
  lua_CFunction         replace;
  lua_CFunction         update;
  lua_CFunction         delete;
} wfx_binding_t;

#endif //NGX_WAFFLEX_TYPES_H
