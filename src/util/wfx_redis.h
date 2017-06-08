#ifndef WFX_REDIS_H
#define WFX_REDIS_H

#include <nginx.h>
#include <ngx_http.h>
#include <util/wfx_lua.h>
#include <ngx_wafflex_types.h>

#define WAFFLEX_REDIS_DEFAULT_URL "127.0.0.1:6379"
#define WAFFLEX_REDIS_DEFAULT_PING_INTERVAL_TIME 30

int wfx_redis_add_server_conf(ngx_conf_t *cf, wfx_loc_conf_t *lcf);
ngx_int_t ngx_wafflex_init_redis(void);
int wfx_redis_init_runtime(lua_State *L, int manager);

#endif //WFX_REDIS_H
