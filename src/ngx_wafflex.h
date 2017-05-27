#ifndef NGX_WAFFLEX_H
#define NGX_WAFFLEX_H

#include <nginx.h>
#include <ngx_http.h>
#include <ngx_wafflex_types.h>
#include <util/wfx_lua.h>
#include <util/ipc.h>
#include <util/shmem.h>

#define WAFFLEX_DEFAULT_SHM_SIZE 33554432  //"32 megs will do", they said...

//#define DEBUG_ON

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

//buncha globals
ipc_t        *wfx_ipc;
lua_State    *wfx_Lua;
shmem_t      *wfx_shm;

void *wfx_shm_alloc(size_t sz);
void *wfx_shm_calloc(size_t sz);
void  wfx_shm_free(void *ptr);

//initialization stuff

ngx_int_t ngx_wafflex_init_lua(void);
ngx_int_t ngx_wafflex_shutdown_lua(void);
ngx_int_t ngx_wafflex_inject_http_filters(void);

void ngx_wafflex_ipc_alert_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data);

extern ngx_module_t ngx_wafflex_module;

#endif //NGX_WAFFLEX_H
