ngx_int_t wfx_init_timer(ngx_event_t *ev, void (*cb)(ngx_event_t *), void *pd);
ngx_event_t *wfx_add_reusable_timer(int (*cb)(void *), size_t data_sz, ngx_msec_t delay);
ngx_event_t *wfx_add_oneshot_timer(void (*cb)(void *), size_t data_sz, ngx_msec_t delay);
ngx_int_t ipc_alert_cachemanager(ipc_t *ipc, ngx_str_t *name, ngx_str_t *data);
ngx_int_t wfx_ipc_alert_cachemanager(const char *name, void *data, size_t sz);
ngx_int_t wfx_ipc_alert_slot(int slot, const char *name, void *data, size_t sz);
ngx_int_t wfx_ipc_set_alert_handler(const char *name, ipc_alert_handler_pt handler);

double wfx_atof(u_char *line, ssize_t n);
ssize_t wfx_parse_size(ngx_str_t *line);
char *wfx_conf_set_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
