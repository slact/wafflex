#include <ngx_wafflex.h>
#include "common.h"
#include "ruleset.h"
#include <assert.h>

#define WFX_WRITE_PENDING 1
#define WFX_WRITE_ACTIVE 2
#define WFX_READ_DISALLOWED -200000 //must be a large negative number for the lock-less magic we perform


typedef struct {
  wfx_readwrite_t     *rw;
  wfx_evaldata_t      *ed;
} readwrite_alert_data_t;

static void readwrite_read_suspend_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data) {
  //run by the manager
  readwrite_alert_data_t     *d = (readwrite_alert_data_t *)data->data;
  wfx_lua_getfunction(wfx_Lua, "readwriteSuspendRead");
  lua_pushlightuserdata(wfx_Lua, d->rw);
  lua_pushnumber(wfx_Lua, sender);
  lua_pushlightuserdata(wfx_Lua, d->ed);
  lua_ngxcall(wfx_Lua, 3, 0);
}

static void readwrite_read_resume_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data) {
  readwrite_alert_data_t     *d = (readwrite_alert_data_t *)data->data;
  if(wfx_tracked_request_active(d->ed)) {
    wfx_resume_suspended_request(d->ed);
  }
  else {
    DBG("request went away");
  }
}

static void readwrite_write_resume_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data) {
  wfx_readwrite_t     *rw = (wfx_readwrite_t *)data->data;
  wfx_lua_getfunction(wfx_Lua, "readwriteResumeWrite");
  lua_pushlightuserdata(wfx_Lua, rw);
  lua_ngxcall(wfx_Lua, 1, 0);
}

static int lua_send_ipc_read_resume_alert(lua_State *L) {
  readwrite_alert_data_t   d;
  ngx_int_t                worker_slot = lua_tonumber(L, 1);
  d.rw = NULL;
  d.ed = lua_touserdata(L, 2);
  
  wfx_ipc_alert_slot(worker_slot, "readwrite-read-resume", &d, sizeof(d));
  return 0;
}

ngx_int_t wfx_readwrite_init_runtime(lua_State *L, int manager) {
  if(manager) {
    wfx_lua_loadscript(L, init);
    lua_pushcfunction(L, lua_send_ipc_read_resume_alert);
    lua_ngxcall(L, 1, 0);
    
    wfx_ipc_set_alert_handler("readwrite-read-suspend", &readwrite_read_suspend_handler);
    wfx_ipc_set_alert_handler("readwrite-write-resume", &readwrite_write_resume_handler);
  }
  else {
    wfx_ipc_set_alert_handler("readwrite-read-resume", &readwrite_read_resume_handler);
  }
  return NGX_OK;
}

int ruleset_common_reserve_read(wfx_evaldata_t *ed, wfx_readwrite_t *rw) {
  int val, defer = 0;
  if(rw->writing > 0) {
    //don't even try
    defer = 1;
  }
  else {
    if(rw->reading >= 0) { // cheap reading-disallowed check
      val = ngx_atomic_fetch_add(&rw->reading, 1);
      if(val < 0) { // reading disallowed
        //deferring
        ngx_atomic_fetch_add(&rw->reading, -1);
        defer = 1;
      }
    }
  }
  
  if(!defer) {
    return 1;
  }
  else {
    wfx_track_request(ed);
    wfx_ipc_alert_cachemanager("readwrite-read-suspend", &ed, sizeof(ed));
    return 0;
  }
}

int ruleset_common_release_read(wfx_readwrite_t *rw) {
  int pending_write = rw->writing == WFX_WRITE_PENDING;
  int val = ngx_atomic_fetch_add(&rw->reading, -1);
  pending_write = pending_write || (rw->writing == WFX_WRITE_PENDING);
  
  if(pending_write && val == 0) {
    wfx_ipc_alert_cachemanager("readwrite-write-resume", &rw, sizeof(rw));
  }
  return 1;
}

int ruleset_common_reserve_write(wfx_readwrite_t *rw) {
  if(!ngx_atomic_cmp_set(&rw->writing, 0, WFX_WRITE_PENDING)) {
    ERR("WRITE already pending or reserved");
    return rw->writing == WFX_WRITE_PENDING ? 0 : 1;
  }
  
  if(!ngx_atomic_cmp_set(&rw->reading, 0, WFX_READ_DISALLOWED)) {
    ERR("non-zero amount of readers (%i), wait until they're done",  rw->reading);
  }
  
  return 1;
}

int ruleset_common_delay_update(lua_State *L, wfx_readwrite_t *rw, lua_CFunction update) {
  wfx_lua_getfunction(L, "readwriteSuspendWrite");
  lua_pushcfunction(L, update);
  lua_pushvalue(L, 1);
  lua_pushvalue(L, 2);
  lua_ngxcall(L, 3, 0);
  return 1;
}

int ruleset_common_release_write(wfx_readwrite_t *rw) {
  assert(rw->reading < 0);
  rw->writing = 0;
  ngx_atomic_fetch_add(&rw->reading, -(WFX_READ_DISALLOWED));
  
  wfx_lua_getfunction(wfx_Lua, "readwriteResumeRead");
  lua_pushlightuserdata(wfx_Lua, rw);
  lua_ngxcall(wfx_Lua, 1, 0);
  
  return 1;
}
