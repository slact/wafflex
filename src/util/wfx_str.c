#include <nginx.h>
#include <ngx_wafflex.h>
#include "wfx_str.h"
#include <assert.h>


ngx_str_t *wfx_http_interpolate_str(wfx_str_t *wstr, ngx_http_request_t *r) {
  return NULL;
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


