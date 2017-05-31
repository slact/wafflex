#include <ngx_wafflex.h>

typedef struct {
  ngx_event_t    ev;
  void          (*cb)(void *pd);
} oneshot_timer_t;

void oneshot_timer_callback(ngx_event_t *ev) {
  oneshot_timer_t  *timer = container_of(ev, oneshot_timer_t, ev);
  timer->cb(ev->data);
  ngx_free(timer);
 }

ngx_event_t *wfx_add_oneshot_timer(void (*cb)(void *), void *pd, ngx_msec_t delay) {
  oneshot_timer_t *timer = ngx_alloc(sizeof(*timer), ngx_cycle->log);
  ngx_memzero(&timer->ev, sizeof(timer->ev));
  timer->cb = cb;
  wfx_init_timer(&timer->ev, oneshot_timer_callback, pd);
  ngx_add_timer(&timer->ev, delay);
  return &timer->ev;
}

ngx_int_t wfx_init_timer(ngx_event_t *ev, void (*cb)(ngx_event_t *), void *pd) {
#if nginx_version >= 1008000
  ev->cancelable = 1;
#endif
  ev->handler = cb;
  ev->data = pd;
  ev->log = ngx_cycle->log;
  return NGX_OK;
}
