#include <ngx_wafflex.h>

typedef struct {
  ngx_event_t    ev;
  int            (*cb)(void *pd);
} custom_timer_t;

void oneshot_timer_callback(ngx_event_t *ev) {
  custom_timer_t  *timer = container_of(ev, custom_timer_t, ev);
  timer->cb(ev->data);
  ngx_free(timer);
}


void reusable_timer_callback(ngx_event_t *ev) {
  int ret;
  custom_timer_t  *timer = container_of(ev, custom_timer_t, ev);  
  if (!ev->timedout) {
    ret = 0;
  }
  else {
    ret = timer->cb(ev->data);
  }
  
  if(ret > 0) {
    ev->timedout = 0;
    ngx_add_timer(&timer->ev, ret);
  }
  else {
    ngx_free(timer);
  }
}

static ngx_event_t *wfx_add_custom_timer(void (*timer_cb)(ngx_event_t *ev), int (*cb)(void *), size_t data_sz, ngx_msec_t delay) {
  custom_timer_t *timer = ngx_alloc(sizeof(*timer) + data_sz, ngx_cycle->log);
  if(!timer) {
    return NULL;
  }
  ngx_memzero(&timer->ev, sizeof(timer->ev));
  timer->cb = cb;
  wfx_init_timer(&timer->ev, timer_cb, data_sz > 0 ? (void *)&timer[1] : NULL);
  ngx_add_timer(&timer->ev, delay);
  return &timer->ev;
}
 
ngx_event_t *wfx_add_reusable_timer(int (*cb)(void *), size_t data_sz, ngx_msec_t delay) {
  return wfx_add_custom_timer(reusable_timer_callback, cb, data_sz, delay);
}
 
ngx_event_t *wfx_add_oneshot_timer(void (*cb)(void *), size_t data_sz, ngx_msec_t delay) {
  return wfx_add_custom_timer(oneshot_timer_callback, (int (*)(void *))cb, data_sz, delay);
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
    //assert(slot_count == 1);
    slot = slots[0];
  }
  return ipc_alert_slot(ipc, slot, name, data);
}

ngx_int_t wfx_ipc_alert_all_workers(const char *name, void *data, size_t sz) {
  ngx_str_t namestr, datastr;
  
  namestr.len = strlen(name);
  namestr.data = (u_char *)name;
  datastr.len = sz;
  datastr.data = (u_char *)data;
  
  return ipc_alert_all_workers(wfx_ipc, &namestr, &datastr);
}

ngx_int_t wfx_ipc_alert_cachemanager(const char *name, void *data, size_t sz) {
  ngx_str_t namestr, datastr;
  
  namestr.len = strlen(name);
  namestr.data = (u_char *)name;
  
  datastr.len = sz;
  datastr.data = (u_char *)data;
  return ipc_alert_cachemanager(wfx_ipc, &namestr, &datastr);
}
ngx_int_t wfx_ipc_alert_slot(int slot, const char *name, void *data, size_t sz) {
  ngx_str_t namestr, datastr;
  
  namestr.len = strlen(name);
  namestr.data = (u_char *)name;
  
  datastr.len = sz;
  datastr.data = (u_char *)data;
  return ipc_alert_slot(wfx_ipc, slot, &namestr, &datastr);
}

ngx_int_t wfx_ipc_set_alert_handler(const char *name, ipc_alert_handler_pt handler) {
  wfx_lua_getfunction(wfx_Lua, "setAlertHandler");
  lua_pushstring(wfx_Lua, name);
  lua_pushlightuserdata(wfx_Lua, handler);
  lua_ngxcall(wfx_Lua, 2, 0);
  return NGX_OK;
}

//converts string to positive double float
double wfx_atof(u_char *line, ssize_t n) {
  ssize_t cutoff, cutlim;
  double  value = 0;
  
  u_char *decimal, *cur, *last = line + n;
  
  if (n == 0) {
    return NGX_ERROR;
  }

  cutoff = NGX_MAX_SIZE_T_VALUE / 10;
  cutlim = NGX_MAX_SIZE_T_VALUE % 10;
  
  decimal = memchr(line, '.', n);
  
  if(decimal == NULL) {
    decimal = line + n;
  }
  
  for (n = decimal - line; n-- > 0; line++) {
    if (*line < '0' || *line > '9') {
      return NGX_ERROR;
    }

    if (value >= cutoff && (value > cutoff || (*line - '0') > cutlim)) {
      return NGX_ERROR;
    }

    value = value * 10 + (*line - '0');
  }
  
  double decval = 0;
  
  for(cur = (decimal - last) > 10 ? decimal + 10 : last-1; cur > decimal && cur < last; cur--) {
    if (*cur < '0' || *cur > '9') {
      return NGX_ERROR;
    }
    decval = decval / 10 + (*cur - '0');
  }
  value = value + decval/10;
  
  return value;
}

ssize_t wfx_parse_size(ngx_str_t *line) {
  u_char   unit;
  size_t   len;
  ssize_t  size, scale, max;
  double   floaty;
  
  len = line->len;
  unit = line->data[len - 1];

  switch (unit) {
  case 'K':
  case 'k':
      len--;
      max = NGX_MAX_SIZE_T_VALUE / 1024;
      scale = 1024;
      break;

  case 'M':
  case 'm':
      len--;
      max = NGX_MAX_SIZE_T_VALUE / (1024 * 1024);
      scale = 1024 * 1024;
      break;
  
  case 'G':
  case 'g':
      len--;
      max = NGX_MAX_SIZE_T_VALUE / (1024 * 1024 * 1024);
      scale = 1024 * 1024 * 1024;
      break;

  default:
      max = NGX_MAX_SIZE_T_VALUE;
      scale = 1;
  }

  floaty = wfx_atof(line->data, len);
  
  if (floaty == NGX_ERROR || floaty > max) {
      return NGX_ERROR;
  }

  size = floaty * scale;

  return size;
}

//config helpers
char *wfx_conf_set_size_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  char  *p = conf;

  size_t           *sp;
  ngx_str_t        *value;
  ngx_conf_post_t  *post;


  sp = (size_t *) (p + cmd->offset);
  if (*sp != NGX_CONF_UNSET_SIZE) {
      return "is duplicate";
  }

  value = cf->args->elts;

  *sp = wfx_parse_size(&value[1]);
  if (*sp == (size_t) NGX_ERROR) {
    return "invalid value";
  }

  if (cmd->post) {
    post = cmd->post;
    return post->post_handler(cf, post, sp);
  }

  return NGX_CONF_OK;
}


wfx_request_ctx_t *wfx_get_request_ctx(wfx_evaldata_t *ed) {
  switch(ed->type) {
    case WFX_EVAL_HTTP_REQUEST:
      return ngx_http_get_module_ctx(ed->data.request, ngx_wafflex_module);
    default:
      ERR("how do?");
      raise(SIGABRT);
      return NULL;
  }
  return NULL;
}


static void wfx_track_request_cleanup_handler(void *d) {
  wfx_lua_getfunction(wfx_Lua, "untrackPtr");
  lua_pushlightuserdata(wfx_Lua, d);
  lua_ngxcall(wfx_Lua, 1, 0);
}

int wfx_track_request(wfx_evaldata_t *ed) {
  wfx_lua_getfunction(wfx_Lua, "trackPtr");
  switch(ed->type) {
    case WFX_EVAL_HTTP_REQUEST:
      lua_pushlightuserdata(wfx_Lua, ed->data.request);
      break;
    default:
      ERR("how do?");
      raise(SIGABRT);
  }
  lua_ngxcall(wfx_Lua, 1, 1);
  if(lua_toboolean(wfx_Lua, -1)) {
    //first time it was added. add cleanup stuff
    ngx_http_cleanup_t *cln = ngx_http_cleanup_add(ed->data.request, 0);
    if(cln == NULL) {
      return 0;
    }
    cln->data = ed->data.request;
    cln->handler = wfx_track_request_cleanup_handler;
  }
  return 1;
}

int wfx_tracked_request_active(wfx_evaldata_t *ed) {
  wfx_lua_getfunction(wfx_Lua, "getTrackedPtr");
  switch(ed->type) {
    case WFX_EVAL_HTTP_REQUEST:
      lua_pushlightuserdata(wfx_Lua, ed->data.request);
      break;
    default:
      ERR("how do?");
      raise(SIGABRT);
  }
  lua_ngxcall(wfx_Lua, 1, 1);
  return lua_toboolean(wfx_Lua, -1);
}

void wfx_resume_suspended_request(wfx_evaldata_t *ed) {
 switch(ed->type) {
    case WFX_EVAL_HTTP_REQUEST:
      ngx_http_core_run_phases(ed->data.request);
      return;
    default:
      ERR("i can't do that yet, Dave.");
      raise(SIGABRT); 
  } 
}

int wfx_util_init_runtime(lua_State *L, int manager) {
  wfx_lua_loadscript(L, util);
  return 1;
}
