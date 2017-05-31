#ifndef WFX_STR_H
#define WFX_STR_H
#include <ngx_wafflex_types.h>

int wfx_str_each_part(ngx_str_t *str, u_char **curptr, wfx_str_part_t *part);
int wfx_lua_set_str_data(lua_State *L, int index, wfx_data_t *wdata);
wfx_str_t *wfx_lua_get_str_binding(lua_State *L, int index);
int wfx_str_sha1(wfx_str_t *wstr, wfx_evaldata_t *ed, u_char *out);

void wfx_str_http_get_part_value(wfx_str_t *wstr, wfx_str_part_t *part, ngx_str_t *out, ngx_http_request_t *r);
void wfx_str_get_part_value(wfx_str_t *wstr, wfx_str_part_t *part, ngx_str_t *out, wfx_evaldata_t *ed);

ngx_str_t *wfx_http_interpolate_string(wfx_str_t *wstr, ngx_http_request_t *r);
#endif
