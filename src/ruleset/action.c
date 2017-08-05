#include <ngx_wafflex.h>
#include "common.h"
#include "action.h"
#include <util/wfx_str.h>

static wfx_action_type_t action_types[];

int action_simple_destroy(lua_State *L) {
  wfx_action_t *action = lua_touserdata(L, 1);
  ruleset_common_shm_free_custom_name_item(L, action, action);
  return 0;
}

wfx_action_t *action_create(lua_State *L, size_t data_sz, wfx_action_eval_pt eval) {
  wfx_action_t *action = ruleset_common_shm_alloc_init_custom_name_item(wfx_action_t, data_sz, L, action);
  action->eval = eval;
  lua_pushlightuserdata(L, action);
  return action;
}

#define action_to_binding(action, binding, buf) \
  snprintf(buf, 255, "action:%s", action->name); \
  (binding).name = buf; \
  (binding).create = action->create; \
  (binding).update = NULL; \
  (binding).delete = action->destroy

void wfx_action_bindings_set(lua_State *L) {
  char     buf[255];
  wfx_action_type_t  *cur;
  wfx_binding_t         binding;
  for(cur = &action_types[0]; cur->name != NULL; cur = &cur[1]) {
    action_to_binding(cur, binding, buf);
    wfx_lua_binding_set(L, &binding);
  }
}
void wfx_action_binding_add(lua_State *L, wfx_action_type_t *cond) {
  char           buf[255];
  wfx_binding_t  binding;
  action_to_binding(cond, binding, buf);
  wfx_lua_binding_set(L, &binding);
}

//some actions

//accept
static wfx_rc_t action_accept_eval(wfx_action_t *self, wfx_evaldata_t *ed) {
  //TODO
  DBG("ACTION: accept");
  return WFX_ACCEPT;
}
static int action_accept_create(lua_State *L) {
  wfx_action_t *action = action_create(L, 0, action_accept_eval);
  action->data.count = 0;
  return 1;
}


typedef struct {
  ngx_int_t  status;
  ngx_str_t  body;
} wfx_action_reject_data_t;

//reject
static wfx_rc_t action_reject_eval(wfx_action_t *self, wfx_evaldata_t *ed) {
  //TODO
  DBG("ACTION: reject");
  wfx_action_reject_data_t *reject_data = self->data.data.ptr;
  ngx_http_request_t       *r;
  
  switch(ed->type) {
    case WFX_EVAL_HTTP_REQUEST:
      r = ed->data.request;
      r->headers_out.status = reject_data->status;
      if(reject_data->body.len > 0) {
        ngx_chain_t    chain;
        ngx_buf_t     *b = ngx_calloc_buf(r->pool);
        if(b == NULL) {
          ERR("failed to allocate buf for REJECT action");
          return WFX_ERROR;
        }
        b->pos = reject_data->body.data;
        b->last = reject_data->body.data + reject_data->body.len;
        b->start = b->pos;
        b->end = b->last;
        b->memory = 1;
        b->last_buf = 1;
        
        r->headers_out.content_length_n = reject_data->body.len;
        
        chain.buf = b;
        chain.next = NULL;
        
        ngx_http_send_header(r);
        ngx_http_output_filter(r, &chain);
      }
      break;
    
    default:
      //do nothing
      break;
  }
  
  return WFX_REJECT;
}
static int action_reject_create(lua_State *L) {
  wfx_action_reject_data_t *reject_data;
  ngx_int_t                 status;
  ngx_str_t                 body;
  
  lua_getfield(L, 1, "data");
  if(lua_istable(L, -1)) {
    lua_getfield(L, -1, "status");
    status = lua_tonumber(L, -1);
    lua_pop(L, 1);
    
    lua_getfield(L, -1, "body");
    lua_tongxstr(L, -1, &body);
    lua_pop(L, 1);
  }
  else {
    status = 403;
    ngx_str_null(&body);
  }
  
  wfx_action_t *action = action_create(L, sizeof(*reject_data) + body.len, action_reject_eval);
  reject_data = (void *)&action[1];
  action->data.data.ptr = reject_data;
  action->data.count = 1;
  
  reject_data->status = status;
  reject_data->body = body;
  if(body.len > 0) {
    reject_data->body.data = (u_char *)&reject_data[1];
    ngx_memcpy(reject_data->body.data, body.data, body.len);
  }
  
  //TODO
  return 1;
}


//wait
typedef struct {
  ngx_http_request_t *r;
  ngx_event_t         ev;
} wait_request_data_t;

static void wait_cleanup(void *d) {
  wait_request_data_t  *wait_data = d;
  if(wait_data->r) {
    ngx_del_timer(&wait_data->ev);
  }
}

static void wait_timer_callback(ngx_event_t *ev) {
  wait_request_data_t  *wait_data = ev->data;
  ngx_http_request_t    *r = wait_data->r;
  wfx_request_ctx_t     *ctx = ngx_http_get_module_ctx(r, ngx_wafflex_module);
  ctx->rule.action.data = (void *)(uintptr_t )1;
  wait_data->r = NULL;
  ngx_http_core_run_phases(r);
}

static wfx_rc_t action_wait_eval(wfx_action_t *self, wfx_evaldata_t *ed) {
  ngx_http_request_t    *r = ed->data.request;
  wfx_request_ctx_t     *ctx = ngx_http_get_module_ctx(r, ngx_wafflex_module);
  ngx_http_cleanup_t    *cln;
  
  if(!ctx || ctx->nocheck || !ctx->rule.action.data) {
    DBG("ACTION: wait start");
    //ERR("wait %i msec...", self->data.data.integer);
    cln = ngx_http_cleanup_add(r, sizeof(wait_request_data_t));
    wait_request_data_t  *wait_data = cln->data;
    wait_data->r = r;
    cln->handler = wait_cleanup;
    
    ngx_memzero(&wait_data->ev, sizeof(wait_data->ev));
    wfx_init_timer(&wait_data->ev, wait_timer_callback, wait_data);
    ngx_add_timer(&wait_data->ev, self->data.data.integer);
    
    return WFX_DEFER;
  }
  else {
    //ERR("no more waiting!");
    DBG("ACTION: wait finish");
    ctx->rule.action.data = (void *)0;
    return WFX_OK;
  }
}
static int action_wait_create(lua_State *L) {
  int           wait_msec;
  wfx_action_t *action;
  
  lua_getfield(L, 1, "data");
  wait_msec = lua_tonumber(L, -1);
  lua_pop(L, 1);
  
  action = action_create(L, 0, action_wait_eval);
  
  action->data.type = WFX_DATA_INTEGER;
  action->data.count = 1;
  action->data.data.integer = wait_msec;
  
  return 1;
}


static wfx_action_type_t action_types[] = {
  {"accept", action_accept_create, NULL},
  {"reject", action_reject_create, NULL},
  {"wait", action_wait_create, NULL},
  {NULL, NULL, NULL}
};
