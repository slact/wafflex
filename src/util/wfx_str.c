#include <nginx.h>
#include <ngx_wafflex.h>
#include "wfx_str.h"
#include <assert.h>


ngx_str_t *wfx_http_interpolate_str(wfx_str_t *wstr, ngx_http_request_t *r) {
  return NULL;
}

int wfx_lua_set_str_data(lua_State *L, int index, wfx_data_t *wdata) {
  wfx_str_t *wstr;
  lua_getfield(L, index, "data");
  
  wstr = wfx_lua_get_str_binding(L, -1);
  
  if(wstr) {
    wdata->data.str = wstr;
  }
  
  lua_pop(L, 1);
  
  wdata->type = WFX_DATA_STRING;
  wdata->count = 1;
  return 1;
}

wfx_str_t *wfx_lua_get_str_binding(lua_State *L, int index) {
  wfx_str_t *wstr;
  
  lua_getfield(L, index, "string");
  if(!lua_isstring(L, -1) || lua_isnumber(L, -1)) {
    luaL_error(L, "expected  a processed string, got something weird instead...");
    return 0;
  }
  lua_pop(L, 1);
  
  lua_getfield(L, index, "__binding");
  wstr = lua_touserdata(L, -1);
  lua_pop(L, 1);
  return wstr;
}

int wfx_str_each_part(ngx_str_t *str, u_char **curptr, wfx_str_part_t *part) {
  u_char *cur = *curptr;
  u_char *first;
  u_char *max = &str->data[str->len];
  int     bracket = 0;
  int     variable = 0;
  int     regex_capture = 0;
  u_char ch;
  if(cur >= max) {
    return 0;
  }
  //assumes a valid-syntax interpolation string
  if(*cur == '$') {
    variable = 1;
    cur++;
    if(cur < max && *cur =='{') {
      bracket = 1;
      cur++;
    }
    first = cur;
    if (*cur >= '1' && *cur <= '9') {
      //nginx only does captures 1-9, no more
      regex_capture = 1;
      cur+= bracket ? 2 : 1;
    }
    else {
      for(first = cur; cur < max; cur++) {
        ch = *cur;
        if ((ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9')
        || ch == '_')
        {
          continue;
        }
        else if(bracket && ch == '}') {
          cur++;
          break;
        }
        else {
          break;
        }
      }
    }
  }
  else {
    first = cur;
    cur = memchr(first, '$', max - first);
    if(cur == NULL) {
      cur = max;
    }
  }
  if(part) {
    part->start = first - str->data;
    part->end = (bracket ? cur - 1 : cur) - str->data;
    part->variable = variable;
    part->regex_capture = regex_capture;
    part->key = 0;
  }
  *curptr = cur;
  return 1;
}


