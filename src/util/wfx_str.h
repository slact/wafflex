#ifndef WFX_STR_H
#define WFX_STR_H
#include <ngx_wafflex_types.h>

int wfx_str_each_part(ngx_str_t *str, u_char **curptr, wfx_str_part_t *part);
int wfx_lua_set_str_data(lua_State *L, int index, wfx_data_t *wdata);
wfx_str_t *wfx_lua_get_str_binding(lua_State *L, int index);


ngx_str_t *wfx_http_interpolate_string(wfx_str_t *wstr, ngx_http_request_t *r);
#endif
