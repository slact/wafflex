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

ngx_int_t ipc_alert_cachemanager(ipc_t *ipc, ngx_str_t *name, ngx_str_t *data) {
  static ngx_int_t slot = NGX_ERROR;
  if(slot == NGX_ERROR) {
    int         slot_count;
    ngx_int_t  *slots;
    slots = ipc_get_process_slots(ipc, &slot_count, IPC_NGX_PROCESS_CACHE_MANAGER);
    assert(slot_count == 1);
    slot = slots[0];
  }
  return ipc_alert_slot(ipc, slot, name, data);
}
