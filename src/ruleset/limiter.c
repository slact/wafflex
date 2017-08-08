#include <ngx_wafflex.h>
#include "common.h"
#include "limiter.h"
#include "condition.h"
#include <util/wfx_str.h>
#include "tracer.h"
#include <assert.h>


#define DEFAULT_SYNC_STEPS 4
#define LIMITER_USED_UPDATE_INTERVAL 2000
#define LIMITER_NAME_MAXLEN 256
#define LIMITER_VALUE_CHECKS_PER_INTERVAL 4

static ngx_int_t limiter_value_ready_to_reap(wfx_limiter_value_t *lval, wfx_limiter_t *limiter, uint8_t force);
static void limiter_reap_value(wfx_limiter_value_t *lval, wfx_limiter_t *limiter);

typedef struct {
  wfx_limiter_t       *limiter;
  wfx_limiter_value_t *val;
  wfx_evaldata_t       ed;
  int                  forced;
  u_char               key[20];
} limit_val_alert_t;

static wfx_limiter_value_t *create_shm_limiter_value(u_char *key) {
  wfx_limiter_value_t *val = wfx_shm_alloc(sizeof(*val));
  if(!val) {
    ERR("failed to allocate limiter");
    return NULL;
  }
  val->time = ngx_time();
  val->count = 0;
  val->refcount = 0;
  ngx_memcpy(&val->key, key, 20);
  
  return val;
}
static void destroy_shm_limiter_value(wfx_limiter_value_t *val) {
  wfx_shm_free(val);
}

static int limiter_current_value(wfx_limiter_t *limiter, wfx_limiter_value_t *lval) {
  int dt = ngx_time() - lval->time;
  int realcount = lval->count - limiter->limit * (float )((float )dt / limiter->interval);
  return realcount < 0 ? 0 : realcount;
}

static int limiter_create(lua_State *L) {
  //expecting a limiter table at top of stack
  wfx_limiter_t     *limiter;
  limiter = ruleset_common_shm_alloc_init_item(wfx_limiter_t, 0, L);
  if(limiter == NULL) {
    luaL_error(L, "failed to initialize limiter: out of memory");
    return 0;
  }
  
  lua_getfield(L, -1, "limit");
  limiter->limit = lua_tonumber(L, -1);
  lua_pop(L,1);
  
  lua_getfield(L, -1, "interval");
  limiter->interval = lua_tonumber(L, -1);
  lua_pop(L,1);
  
  lua_getfield(L, -1, "burst");
  if(lua_isnil(L, -1)) {
    limiter->burst.limiter = NULL;
    limiter->burst.expire = 0;
  }
  else {
    lua_getfield(L, -1, "__binding");
    limiter->burst.limiter = lua_touserdata(L, -1);
    lua_pop(L, 1);
    
    lua_getfield(L, -2, "burst_expire");
    limiter->burst.expire = lua_isnil(L, -1) ? -1 : lua_tonumber(L, -1);
    lua_pop(L, 1);
  }
  
  limiter->values = ngx_calloc(sizeof(*limiter->values), ngx_cycle->log);
  if(!limiter) {
    luaL_error(L, "unable to allocate limiter values reaper");
    return 0;
  }
  reaper_start(limiter->values, 
                "limiter values reaper", 
                offsetof(wfx_limiter_value_t, prev), 
                offsetof(wfx_limiter_value_t, next), 
  (ngx_int_t (*)(void *, void *, uint8_t)) limiter_value_ready_to_reap,
        (void (*)(void *, void *)) limiter_reap_value,
                limiter,
                (limiter->interval * 1000) / LIMITER_VALUE_CHECKS_PER_INTERVAL
  );
  limiter->values->strategy = REAPER_ROTATE;
  limiter->values->max_notready_ratio = 0.3;
  
  lua_pop(L, 1);
  
  lua_pushlightuserdata(L, limiter);
  return 1;
}

static int limiter_update(lua_State *L) {
  // stack index 2: delta table
  // stack index 1: rule (userdata)
  
  wfx_limiter_t     *limiter = lua_touserdata(L, 1);
  
  assert(limiter->rw.reading == 0);
  assert(limiter->rw.writing == 1);
  limiter->rw.writing = 2;
  
  ruleset_common_update_item_name(L, &limiter->name);
  
  lua_getfield(L, 2, "limit");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    limiter->limit = lua_tonumber(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  lua_getfield(L, 2, "interval");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    limiter->interval = lua_tonumber(L, -1);
    limiter->values->tick_usec = (limiter->interval * 1000) / LIMITER_VALUE_CHECKS_PER_INTERVAL;
    //TODO: maybe restart timer?
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  lua_getfield(L, 2, "burst");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    if(lua_isnil(L, -1)) {
      limiter->burst.limiter = NULL;
    }
    else {
      lua_getfield(L, -1, "__binding");
      limiter->burst.limiter = lua_touserdata(L, -1);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  lua_getfield(L, 2, "burst_expire");
  if(!lua_isnil(L, -1)) {
    lua_getfield(L, -1, "new");
    limiter->burst.expire = lua_isnil(L, -1) ? -1 : lua_tonumber(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  limiter->rw.writing = 0;
  return 0;
}

static int limiter_delete(lua_State *L) {
  wfx_limiter_t     *limiter = lua_touserdata(L, 1);
  if (!limiter) {
    lua_printstack(L);
    luaL_error(L, "expected limiter __binding to be some value, bit got NULL");
    return 0;
  }
  
  reaper_stop(limiter->values);
  ngx_free(limiter->values);
  limiter->values = NULL;
  
  ruleset_common_shm_free_item(L, limiter);
  return 0;
}

static ngx_int_t limiter_value_ready_to_reap(wfx_limiter_value_t *lval, wfx_limiter_t *limiter, uint8_t force) {
  if(force || ((lval->count == 0 || limiter_current_value(limiter, lval) == 0) && lval->time + 1 <= ngx_time())) {
    if(lval->refcount == 0) {
      return NGX_OK;
    }
    else {
      limit_val_alert_t  data;
      
      data.limiter = limiter;
      ngx_memcpy(data.key, lval->key, 20);
      data.val = lval;
      ngx_memzero(&data.ed, sizeof(data.ed));
      data.forced = force;
      
      wfx_ipc_alert_all_workers("limiter-value-release-request", &data, sizeof(data));
      return force ? NGX_OK : NGX_DECLINED;
    }
    return NGX_OK;
  }
  else {
    return NGX_DECLINED;
  }
}
static void limiter_reap_value(wfx_limiter_value_t *lval, wfx_limiter_t *limiter) { 
  wfx_lua_getfunction(wfx_Lua, "unsetLimiterValue");
  lua_pushlightuserdata(wfx_Lua, limiter);
  lua_pushlstring(wfx_Lua, (const char *)lval->key, 20);
  lua_pushlightuserdata(wfx_Lua, lval);
  lua_ngxcall(wfx_Lua, 3, 0);
  destroy_shm_limiter_value(lval);
}

static wfx_binding_t wfx_limiter_binding = {
  "limiter",
  limiter_create,
  limiter_update,
  limiter_delete
};

//limiter conditions
typedef struct {
  wfx_limiter_t   *limiter;
  wfx_str_t       *key;
  int              increment;
} limit_condition_data_t;

static void limiter_value_request_alert_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data) {
  limit_val_alert_t *d = (limit_val_alert_t *)data->data;
  
  wfx_lua_getfunction(wfx_Lua, "findLimiterValue");
  lua_pushlightuserdata(wfx_Lua, d->limiter);
  lua_pushlstring(wfx_Lua, (const char *)d->key, 20);
  lua_ngxcall(wfx_Lua, 2, 1);
  if(!lua_isnil(wfx_Lua, -1)) {
    //already exists
    d->val = lua_touserdata(wfx_Lua, -1);
    d->val->refcount++;
  }
  else if((d->val = create_shm_limiter_value(d->key)) != NULL) {
    //succesfully created
    d->val->refcount++;
    reaper_add(d->limiter->values, d->val);
    wfx_lua_getfunction(wfx_Lua, "setLimiterValue");
    lua_pushlightuserdata(wfx_Lua, d->limiter);
    lua_pushlstring(wfx_Lua, (const char *)d->key, 20);
    lua_pushlightuserdata(wfx_Lua, d->val);
    lua_ngxcall(wfx_Lua, 3, 0);
  }
  lua_pop(wfx_Lua, 1);
  lua_printstack(wfx_Lua);
  
  wfx_ipc_alert_slot(sender, "limiter-value-response", d, sizeof(*d));
}

static void limiter_value_response_alert_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data) {
  limit_val_alert_t *d = (limit_val_alert_t *)data->data;
  
  assert(d->val);
  wfx_lua_getfunction(wfx_Lua, "setLimiterValue");
  lua_pushlightuserdata(wfx_Lua, d->limiter);
  lua_pushlstring(wfx_Lua, (const char *)d->key, 20);
  lua_pushlightuserdata(wfx_Lua, d->val);
  lua_ngxcall(wfx_Lua, 3, 0);
  
  if(wfx_tracked_request_active(&d->ed)) {
    condition_stack_set_tail_data(&d->ed, d->val);
    wfx_resume_suspended_request(&d->ed);
  }
}

static void limiter_value_release_request_alert_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data) {
  limit_val_alert_t *d = (limit_val_alert_t *)data->data;
  wfx_lua_getfunction(wfx_Lua, "unsetLimiterValue");
  lua_pushlightuserdata(wfx_Lua, d->limiter);
  lua_pushlstring(wfx_Lua, (const char *)d->key, 20);
  if(d->val)
    lua_pushlightuserdata(wfx_Lua, d->val);
  else
    lua_pushnil(wfx_Lua);
  lua_ngxcall(wfx_Lua, 3, 1);
  if(lua_toboolean(wfx_Lua, -1)) {
    //unset successful
    wfx_ipc_alert_slot(sender, "limiter-value-release-response", d, sizeof(*d));
  }
  else {
    //limiter value not found here. don't reply.
  }
}

static void limiter_value_release_response_alert_handler(ngx_pid_t sender_pid, ngx_int_t sender, ngx_str_t *name, ngx_str_t *data) {
  limit_val_alert_t *d = (limit_val_alert_t *)data->data;
  
  if(!d->forced) { //not forced
    d->val->refcount--;
    if(d->val->refcount == 0) {
      reaper_withdraw(d->limiter->values, d->val);
      reaper_add(d->limiter->values, d->val);
    }
  }
  else {
    //was already reaped
  }
}

static wfx_condition_rc_t condition_limit_check_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  limit_condition_data_t *data = self->data.data.ptr;
  wfx_limiter_t          *limiter = data->limiter;
  wfx_limiter_value_t    *lval;
  u_char                  key_sha1[20];
  
  int                     realcount;
  
  if(condition_stack_empty(stack)) {
    wfx_str_sha1(data->key, ed, key_sha1);
    tracer_log_wstr(ed, "key", data->key);
    
    wfx_lua_getfunction(wfx_Lua, "findLimiterValue"); //get limit and update recently-used value
    lua_pushlightuserdata(wfx_Lua, data->limiter);
    lua_pushlstring(wfx_Lua, (const char *)key_sha1, 20);
    lua_ngxcall(wfx_Lua, 2, 1);
    
    if(!lua_isnil(wfx_Lua, -1)) {
      lval = lua_touserdata(wfx_Lua, -1);
      lua_pop(wfx_Lua, 1);
    }
    else {
      limit_val_alert_t     alert;
      alert.limiter = data->limiter;
      ngx_memcpy(alert.key, key_sha1, sizeof(key_sha1));
      alert.ed = *ed;
      alert.forced = 0;
      alert.val = NULL;
      
      wfx_track_request(ed);
      wfx_ipc_alert_cachemanager("limiter-value-request", &alert, sizeof(alert));
      
      condition_stack_push(stack, NULL);
      return WFX_COND_DEFER;
    }
  }
  else {
    lval = condition_stack_pop(stack);
    wfx_lua_getfunction(wfx_Lua, "setLimiterValue");
    lua_pushlightuserdata(wfx_Lua, data->limiter);
    lua_pushlstring(wfx_Lua, (const char *)lval->key, 20);
    lua_pushlightuserdata(wfx_Lua, lval);
    lua_ngxcall(wfx_Lua, 3, 0);
  }
  
  //ok, we now have the limiter data. make use of it.
  if(data->increment > 0) {
    ngx_atomic_fetch_add((ngx_atomic_uint_t *)&lval->count, data->increment);
  }
  realcount = limiter_current_value(limiter, lval);
  tracer_log_number(ed, "count", realcount);
  return condition_rc_maybe_negate(self, realcount <= limiter->limit ? WFX_COND_TRUE : WFX_COND_FALSE);
}

static int condition_limit_checkbreak_create(lua_State *L, int default_increment, int negate) {
  limit_condition_data_t *data;
  wfx_condition_t *limit_check = condition_create(L, sizeof(*data), condition_limit_check_eval);
  limit_check->data.type = WFX_DATA_PTR;
  limit_check->data.count = 1;
  limit_check->data.data.ptr = &limit_check[1];
  data = limit_check->data.data.ptr;
  
  lua_getfield(L, -2, "data");
  
  lua_getfield(L, -1, "limiter");
  lua_getfield(L, -1, "__binding");
  data->limiter = lua_touserdata(L, -1);
  lua_pop(L, 2);
  
  lua_getfield(L, -1, "key");
  data->key = wfx_lua_get_str_binding(L, -1);
  lua_pop(L, 1);
  
  lua_getfield(L, -1, "increment");
  if(lua_isnil(L, -1))
    data->increment = default_increment;
  else
    data->increment = lua_tonumber(L, -1);
  lua_pop(L, 1);
  
  lua_pop(L, 1); //pop data
  
  if(negate)
    limit_check->negate = 1;
  
  return 1;
}

static int condition_limit_break_create(lua_State *L) {
  return condition_limit_checkbreak_create(L, 1, 1);
}

static wfx_condition_type_t limit_break = {
  "limit-break",
  condition_limit_break_create,
  condition_simple_destroy
};

static int condition_limit_check_create(lua_State *L) {
  return condition_limit_checkbreak_create(L, 0, 0);
}

static wfx_condition_type_t limit_check = {
  "limit-check",
  condition_limit_check_create,
  condition_simple_destroy
};

void wfx_limiter_bindings_set(lua_State *L) {
  wfx_lua_binding_set(L, &wfx_limiter_binding);
  wfx_condition_binding_add(L, &limit_break);
  wfx_condition_binding_add(L, &limit_check);
}

int wfx_limiter_init_runtime(lua_State *L, int manager) {
  if(manager) {
    wfx_ipc_set_alert_handler("limiter-value-request", limiter_value_request_alert_handler);
    wfx_ipc_set_alert_handler("limiter-value-release-response", limiter_value_release_response_alert_handler);
  }
  else {
    wfx_ipc_set_alert_handler("limiter-value-response", limiter_value_response_alert_handler);
    wfx_ipc_set_alert_handler("limiter-value-release-request", limiter_value_release_request_alert_handler);
  }
  
  wfx_lua_loadscript(L, limiter);
  
  return NGX_OK;
}
