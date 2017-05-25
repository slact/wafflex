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

typedef enum {WFX_EVAL_ACCEPT, WFX_EVAL_HTTP} wfx_evaldata_type_t;
typedef struct {
  wfx_evaldata_type_t  type;
  union {
    ngx_http_request_t   *request;
  }                     data;
} wfx_evaldata_t;

typedef enum {WFX_DATA_INTEGER, WFX_DATA_FLOAT, WFX_DATA_STRING, WFX_DATA_STRING_ARRAY, WFX_DATA_PTR} wfx_data_type_t;
typedef struct {
  wfx_data_type_t   type;
  int               count;
  union {
    void              *ptr;
    wfx_str_t         *str;
    wfx_str_t        *str_array[1];
    ngx_int_t          integer;
    float              floating_point;
  }                 data;
} wfx_data_t;

#endif //NGX_WAFFLEX_TYPES_H
