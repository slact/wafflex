ngx_int_t wfx_init_timer(ngx_event_t *ev, void (*cb)(ngx_event_t *), void *pd);
ngx_event_t *wfx_add_oneshot_timer(void (*cb)(void *), void *pd, ngx_msec_t delay);
