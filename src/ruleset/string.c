#include <ngx_wafflex.h>
#include "common.h"
#include "string.h"
#include <util/wfx_str.h>



static int string_create(lua_State *L) {
  ngx_str_t        str;
  wfx_str_t       *wstr;
  wfx_str_part_t  *part;
  int              parts_count = 0;
  u_char          *cur, *ccur;
  size_t           sz;
  ngx_str_t        spart;
  int              i = 0;
  lua_getfield(L, -1, "string");
  str.data = (u_char *)luaL_tolstring(L, -1, &str.len);
  
  cur = str.data;
  while(wfx_str_each_part(&str, &cur, NULL)) {
    parts_count++;
  }
  sz = str.len + sizeof(*part) * parts_count;
  
  wstr = ruleset_common_shm_alloc_init_item_noname(wfx_str_t, sz, L);
  
  wstr->str.len = str.len;
  wstr->str.data=(u_char *)&wstr[1];
  ngx_memcpy(wstr->str.data, str.data, str.len);
  wstr->parts_count = parts_count;
  wstr->parts = (void *)&wstr->str.data[str.len];
  
  cur = wstr->str.data;
  part = &wstr->parts[0];
  
  while(wfx_str_each_part(&wstr->str, &cur, part)) {
    ccur = &wstr->str.data[part->start];
    spart.data = ccur;
    spart.len = part->end - part->start;
    if(part->variable) {
      if(part->regex_capture) {
        part->key = *ccur - '0';
      }
      else {
        part->key = ngx_hash_strlow(spart.data, spart.data, spart.len);
      }
    } 
    else {
      part->key = ngx_hash_key(spart.data, spart.len);
    }
    
    //DBG("part (%i): %s%s key: %i str: \"%V\"", i, part->variable ? "var" : "raw", part->regex_capture ? " regex capture" : "", part->key, &spart);
    i++;
    part = &wstr->parts[i];
  }
  
  lua_pushlightuserdata(L, wstr);
  return 1;
}
static wfx_binding_t wfx_string_binding = {
  "string",
  string_create,
  NULL,
  NULL,
  NULL
};

void wfx_string_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_string_binding);
}
