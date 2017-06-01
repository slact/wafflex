ngx_int_t wfx_init_timer(ngx_event_t *ev, void (*cb)(ngx_event_t *), void *pd);
ngx_event_t *wfx_add_oneshot_timer(void (*cb)(void *), void *pd, ngx_msec_t delay);
ngx_int_t ipc_alert_cachemanager(ipc_t *ipc, ngx_str_t *name, ngx_str_t *data);
