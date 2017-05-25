#ifndef WFX_STR_H
#define WFX_STR_H
#include <ngx_wafflex_types.h>

int wfx_str_each_part(ngx_str_t *str, u_char **curptr, wfx_str_part_t *part);

ngx_str_t *wfx_http_interpolate_string(wfx_str_t *wstr, ngx_http_request_t *r);
#endif
