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

typedef struct {
  uint16_t       start;
  uint16_t       end;
  ngx_uint_t     key;
  unsigned       variable:1;
  unsigned       regex_capture:1;
} wfx_str_part_t;

typedef struct {
  ngx_str_t        str;
  int              luaref;
  uint16_t         parts_count;
  wfx_str_part_t  *parts;
} wfx_str_t;

#endif //NGX_WAFFLEX_TYPES_H
