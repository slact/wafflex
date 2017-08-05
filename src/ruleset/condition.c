#include <ngx_wafflex.h>
#include "common.h"
#include "condition.h"
#include <util/wfx_str.h>
#include "tracer.h"

#include <assert.h>

static wfx_condition_type_t condition_types[];

int condition_simple_destroy(lua_State *L) {
  wfx_condition_t *cond = lua_touserdata(L, 1);
  ruleset_common_shm_free_custom_name_item(L, cond, condition);
  return 0;
}

int condition_false_destroy(lua_State *L) {
  ERR("KILL THIS CONDITION FALSE");
  return condition_simple_destroy(L);
}

wfx_condition_t *condition_create(lua_State *L, size_t data_sz, wfx_condition_eval_pt eval) {
  wfx_condition_t *condition = ruleset_common_shm_alloc_init_custom_name_item(wfx_condition_t, data_sz, L, condition);
  condition->eval = eval;
  lua_pushlightuserdata(L, condition);
  return condition;
}

int condition_stack_push(wfx_condition_stack_t *stack, void *d) {
  wfx_condition_stack_el_t *el = ngx_alloc(sizeof(*el), ngx_cycle->log);
  if(!el)
    return 0;
  if(!stack->tail)
    stack->tail = el;
  el->next = stack->head;
  el->pd = d;
  stack->head = el;
  return 1;
}

void *condition_stack_pop(wfx_condition_stack_t *stack) {
  wfx_condition_stack_el_t *el = stack->head;
  void                     *d;
  if(!el)
    return NULL;
  d = el->pd;
  stack->head = el->next;
  if(stack->tail == el)
    stack->tail = NULL;
  ngx_free(el);
  return d;
}
void condition_stack_clear(wfx_condition_stack_t *stack) {
  wfx_condition_stack_el_t *el, *nextel;
  for(el = stack->head; el != NULL; el = nextel) {
    nextel = el->next;
    ngx_free(el);
  }
  stack->head = NULL;
  stack->tail = NULL;
}

void condition_stack_set_tail_data(wfx_evaldata_t *ed, void *d) {
  wfx_request_ctx_t  *ctx;
  switch(ed->type) {
    case WFX_EVAL_HTTP_REQUEST:
      ctx = ngx_http_get_module_ctx(ed->data.request, ngx_wafflex_module);
      ctx->rule.condition_stack.tail->pd = d;
      break;
    default:
      ERR("don't know how to do that yet");
      raise(SIGABRT);
  }
}

#define condition_to_binding(cond, binding, buf) \
  snprintf(buf, 255, "condition:%s", cond->name); \
  (binding).name = buf; \
  (binding).create = cond->create; \
  (binding).update = NULL; \
  (binding).delete = cond->destroy

void wfx_condition_bindings_set(lua_State *L) {;
  char     buf[255];
  wfx_condition_type_t  *cur;
  wfx_binding_t         binding;
  for(cur = &condition_types[0]; cur->name != NULL; cur = &cur[1]) {
    condition_to_binding(cur, binding, buf);
    wfx_lua_binding_set(L, &binding);
  }
}
void wfx_condition_binding_add(lua_State *L, wfx_condition_type_t *cond) {
  char           buf[255];
  wfx_binding_t  binding;
  condition_to_binding(cond, binding, buf);
  wfx_lua_binding_set(L, &binding);
}



//and now -- conditions


//true
static wfx_condition_rc_t condition_true_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  DBG("CONDITION: true");
  return condition_rc_maybe_negate(self, WFX_COND_TRUE);
}
static int condition_true_create(lua_State *L) {
  condition_create(L, 0, condition_true_eval);
  return 1;
}

//false
static wfx_condition_rc_t condition_false_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  DBG("CONDITION: false");
  return condition_rc_maybe_negate(self, WFX_COND_FALSE);
}
static int condition_false_create(lua_State *L) {
  condition_create(L, 0, condition_false_eval);
  return 1;
}

//any and all
static int condition_array_create(lua_State *L, wfx_condition_eval_pt eval) {
  size_t                  condition_count;
  wfx_condition_t       **conditions = NULL;
  unsigned                i;
  
  lua_getfield(L, -1, "data");
  condition_count = wfx_lua_len(L, -1);
  lua_pop(L, 1);
  
  wfx_condition_t *condition = condition_create(L, (sizeof(*conditions) * condition_count), eval);
  
  condition->data.type = WFX_DATA_PTR;
  condition->data.count = condition_count;
  condition->data.data.ptr = &condition[1];
  conditions = condition->data.data.ptr;
  
  lua_getfield(L, -2, "data");
  for(i=0; i<condition_count; i++) {
    lua_geti(L, -1, i+1);
    lua_getfield(L, -1, "__binding");
    conditions[i]=lua_touserdata(L, -1);
    lua_pop(L, 2);
  }
  lua_pop(L, 1);
  
  return 1;
}

static wfx_condition_rc_t condition_list_eval(wfx_data_t *data, wfx_condition_rc_t stop_on, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  wfx_condition_t       **conditions = (wfx_condition_t **)data->data.ptr;
  unsigned                len = data->count;
  wfx_condition_t        *cur;
  uintptr_t                start, i;
  wfx_condition_rc_t      rc;
  if(condition_stack_empty(stack)) {
    start = 0;
  }
  else {
    start = (uintptr_t )(char *)condition_stack_pop(stack);
  }
  
  for(i=start; i<len; i++) {
    cur = conditions[i];
    tracer_push(ed, WFX_CONDITION, cur);
    rc = cur->eval(cur, ed, stack);
    tracer_pop(ed, WFX_CONDITION, rc);
    if(rc == stop_on) {
      return stop_on;
    }
    else if(rc == WFX_COND_DEFER) {
      condition_stack_push(stack, (void *)i);
      return rc;
    }
    else if(rc == WFX_COND_ERROR) {
      return rc;
    }
  }
  if(stop_on == WFX_COND_FALSE) {
    return WFX_COND_TRUE;
  }
  else if(stop_on == WFX_COND_TRUE) {
    return WFX_COND_FALSE;
  }
  else {
    return rc;
  }
}

wfx_condition_rc_t condition_rc_maybe_negate(wfx_condition_t *self, wfx_condition_rc_t rc) {
  switch(rc) {
    case WFX_COND_FALSE:
      return self->negate ? WFX_COND_TRUE : WFX_COND_FALSE;      
    case WFX_COND_TRUE:
      return self->negate ? WFX_COND_FALSE : WFX_COND_TRUE;
    case WFX_COND_DEFER:
    case WFX_COND_ERROR:
      //do nothing
      break;
  }
  return rc;
}

//any
static wfx_condition_rc_t condition_any_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  DBG("CONDITION: any");
  wfx_condition_rc_t   rc = condition_list_eval(&self->data, WFX_COND_TRUE, ed, stack);
  return condition_rc_maybe_negate(self, rc);
}
static int condition_any_create(lua_State *L) {
  return condition_array_create(L, condition_any_eval);
}

//all
static wfx_condition_rc_t condition_all_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  DBG("CONDITION: all");
  wfx_condition_rc_t   rc = condition_list_eval(&self->data, WFX_COND_FALSE, ed, stack);
  return condition_rc_maybe_negate(self, rc);
}
static int condition_all_create(lua_State *L) {
  return condition_array_create(L, condition_all_eval);
}

static int condition_match_eval_bool(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  wfx_data_t     *data = &self->data;
  ngx_str_t       str[32];
  
  ngx_str_t       scur;
  ngx_str_t       ccur;
  
  wfx_str_part_t *parts;
  wfx_str_t     **wstrs = data->data.str_array;
  int             i, smax, cmax, si, ci;
  
  int             negate = self->negate;
  int             cmp;

  DBG("CONDITION: match");
  
  if(data->count == 0)
    return 0;
  else if(data->count == 1) //1 string always matches
    return 1; 
  
  smax = wstrs[0]->parts_count;
  parts = wstrs[0]->parts;
  
  if(smax == 0) {
    //some kind of weird zero-part string. This shouldn't happen... 
    //but I guess it's not outright insane
    return 0;
  }
  
  for(si = 0; si < smax; si++) {
    //build first (simplest) string
    wfx_str_get_part_value(wstrs[0], &parts[si], &str[si], ed); 
  }
  tracer_log_wstr_array(ed, "strings", wstrs[0]);
  
  //there's room for optimization here, guys. just not prematurely.
  
  for(i=1; i < data->count; i++) {
    //compare piecemeal against others
    si = 0;
    scur = str[si];

    ci = 0;
    cmax = wstrs[i]->parts_count;
    parts = wstrs[i]->parts;
    wfx_str_get_part_value(wstrs[i], &parts[ci], &ccur, ed); 
    tracer_log_wstr_array(ed, "strings", wstrs[1]);
    
    while(1) {
      if(scur.len < ccur.len) {
        if(scur.len > 0) {
          cmp = memcmp(scur.data, ccur.data, scur.len);
          ccur.data+=scur.len;
          ccur.len -=scur.len;
          
          if(cmp == 0 && !negate)
            return 0;
          if(cmp != 0 && negate)
            return 1;
        }
        
        if(++si < smax)
          scur = str[si];
        else if(ccur.len > 0 && !negate) // unmatched data, no more other string
          return 0;
        else if(ccur.len == 0 && negate) //no leftover unmatched data
          return 0;
        else
          break;
      }
      else {
        if(ccur.len > 0) {
          cmp = memcmp(scur.data, ccur.data, ccur.len);
          scur.data+=ccur.len;
          scur.len -=ccur.len;
          
          if(cmp == 0 && !negate)
            return 0;
          if(cmp != 0 && negate)
            return 1;
        }
        
        if(++ci < cmax)
          wfx_str_get_part_value(wstrs[i], &parts[ci], &ccur, ed); 
        else if(scur.len > 0 && !negate) //unmatched data, no more other string
          return 0;
        else if(scur.len == 0 && negate) //no leftover unmatched data
          return 0;
        else
          break;
      }
    }
  }
  
  return !negate;
}

static wfx_condition_rc_t condition_match_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  return condition_match_eval_bool(self, ed, stack) ? WFX_COND_TRUE : WFX_COND_FALSE;
}

static int condition_match_create(lua_State *L) {
  wfx_condition_t *condition;
  int              i, wstr_count;
  wfx_str_t      **wstr;
  wfx_data_t      *data;
  
  lua_getfield(L, 1, "data");
  wstr_count = wfx_lua_len(L, -1);
  lua_pop(L, 1);
  
  condition = condition_create(L, sizeof(*wstr) * (wstr_count - 1), condition_match_eval);
  
  data = &condition->data;
  
  data->type = WFX_DATA_STRING;
  data->count = wstr_count;
  wstr = data->data.str_array;
  
  lua_getfield(L, 1, "data");
  for(i=0; i < wstr_count; i++) {
    lua_geti(L, -1, i+1);
    wstr[i]=wfx_lua_get_str_binding(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  return 1;
}

//all
typedef struct {
  ngx_http_request_t *r;
  ngx_event_t         ev;
} delay_request_data_t;

static void delay_cleanup(void *d) {
  delay_request_data_t  *delay_data = d;
  if(delay_data->r) {
    ngx_del_timer(&delay_data->ev);
  }
}

static void delay_timer_callback(ngx_event_t *ev) {
  delay_request_data_t  *delay_data = ev->data;
  ngx_http_request_t    *r = delay_data->r;
  delay_data->r = NULL;
  ngx_http_core_run_phases(r);
}

static wfx_condition_rc_t condition_delay_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  ngx_http_request_t    *r = ed->data.request;
  ngx_http_cleanup_t    *cln;
  if(condition_stack_empty(stack)) {
    DBG("CONDITION: .delay start");
    cln = ngx_http_cleanup_add(r, sizeof(delay_request_data_t));
    delay_request_data_t  *delay_data = cln->data;
    delay_data->r = r;
    cln->handler = delay_cleanup;
    
    ngx_memzero(&delay_data->ev, sizeof(delay_data->ev));
    wfx_init_timer(&delay_data->ev, delay_timer_callback, delay_data);
    ngx_add_timer(&delay_data->ev, self->data.data.integer);
    
    condition_stack_push(stack, NULL);
    return WFX_COND_DEFER;
  }
  else {
    DBG("CONDITION: .delay end");
    condition_stack_pop(stack);
    return WFX_COND_TRUE;
  }
}
static int condition_delay_create(lua_State *L) {
  int wait_msec;
  wfx_condition_t *delay;
  lua_getfield(L, 1, "data");
  wait_msec = lua_tonumber(L, -1);
  lua_pop(L, 1);
  
  delay = condition_create(L, 0, condition_delay_eval);
  delay->data.type = WFX_DATA_INTEGER;
  delay->data.count = 1;
  delay->data.data.integer = wait_msec;
  
  return 1;
}


static wfx_condition_type_t condition_types[] = {
  {"true", condition_true_create, condition_simple_destroy},
  {"false", condition_false_create, condition_false_destroy},
  {"any", condition_any_create, condition_simple_destroy},
  {"match", condition_match_create, condition_simple_destroy},
  {"all", condition_all_create, condition_simple_destroy},
  {".delay", condition_delay_create, condition_simple_destroy},
  {NULL, NULL, NULL}
};
