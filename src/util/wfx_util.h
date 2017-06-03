ngx_int_t wfx_init_timer(ngx_event_t *ev, void (*cb)(ngx_event_t *), void *pd);
ngx_event_t *wfx_add_oneshot_timer(void (*cb)(void *), void *pd, ngx_msec_t delay);
ngx_int_t ipc_alert_cachemanager(ipc_t *ipc, ngx_str_t *name, ngx_str_t *data);
ngx_int_t wfx_ipc_alert_cachemanager(const char *name, void *data, size_t sz);
ngx_int_t wfx_ipc_alert_slot(int slot, const char *name, void *data, size_t sz);
ngx_int_t wfx_ipc_set_alert_handler(const char *name, ipc_alert_handler_pt handler);
