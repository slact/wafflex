#ifndef NGX_WAFFLEX_H
#define NGX_WAFFLEX_H

#include <nginx.h>
#include <ngx_http.h>
#include <ngx_wafflex_types.h>
#include <util/wfx_lua.h>

#define DEBUG_ON

#ifdef DEBUG_ON
#define DBG(fmt, args...) ngx_log_error(NGX_LOG_WARN, ngx_cycle->log, 0, "wafflex:" fmt, ##args)
#else
#define DBG(fmt, args...) 
#endif
#define ERR(fmt, args...) ngx_log_error(NGX_LOG_ERR, ngx_cycle->log, 0, "wafflex:" fmt, ##args)

#ifndef container_of
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

void *wfx_shm_alloc(size_t sz);
void *wfx_shm_calloc(size_t sz);
void  wfx_shm_free(void *ptr);

extern ngx_module_t ngx_wafflex_module;

ngx_str_t *wfx_get_interpolated_string(const char *str);


#endif //NGX_WAFFLEX_H
