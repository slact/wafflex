#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "condition.h"
#include <util/wfx_str.h>

#include <assert.h>

static wfx_condition_type_t condition_types[];

wfx_condition_t *condition_create(lua_State *L, size_t data_sz, wfx_condition_eval_pt eval) {
  wfx_condition_t *condition = ruleset_common_shm_alloc_init_item(wfx_condition_t, data_sz, L, condition);
  condition->eval = eval;
  lua_pushlightuserdata(L, condition);
  return condition;
}

int condition_stack_append(wfx_condition_stack_t *stack, void *d) {
  wfx_condition_stack_el_t *el = ngx_alloc(sizeof(*el), ngx_cycle->log);
  if(!el)
    return 0;
  if(stack->tail)
    stack->tail->next = el;
  if(!stack->head)
    stack->head = el;
  stack->tail = el;
  el->next = NULL;
  el->pd = d;
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

#define condition_to_binding(cond, binding, buf) \
  snprintf(buf, 255, "condition:%s", cond->name); \
  (binding).name = buf; \
  (binding).create = cond->create; \
  (binding).update = NULL; \
  (binding).replace = NULL; \
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
  return !self->negate ? WFX_COND_TRUE : WFX_COND_FALSE;
}
static int condition_true_create(lua_State *L) {
  condition_create(L, 0, condition_true_eval);
  return 1;
}

//false
static wfx_condition_rc_t condition_false_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  return self->negate ? WFX_COND_TRUE : WFX_COND_FALSE;
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
    rc = cur->eval(cur, ed, stack);
    if(rc == stop_on) {
      return stop_on;
    }
    else if(rc == WFX_COND_SUSPEND) {
      condition_stack_append(stack, (void *)i);
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

//any
static wfx_condition_rc_t condition_any_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  wfx_condition_rc_t   rc = condition_list_eval(&self->data, WFX_COND_TRUE, ed, stack);
  switch(rc) {
    case WFX_COND_FALSE:
    case WFX_COND_TRUE:
      return self->negate ? !rc : rc;
    case WFX_COND_SUSPEND:
    case WFX_COND_ERROR:
      return rc;
  }
}
static int condition_any_create(lua_State *L) {
  return condition_array_create(L, condition_any_eval);
}

//all
static wfx_condition_rc_t condition_all_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  wfx_condition_rc_t   rc = condition_list_eval(&self->data, WFX_COND_FALSE, ed, stack);
  switch(rc) {
    case WFX_COND_FALSE:
    case WFX_COND_TRUE:
      return self->negate ? !rc : rc;
    case WFX_COND_SUSPEND:
    case WFX_COND_ERROR:
      return rc;
  }
  
}
static int condition_all_create(lua_State *L) {
  return condition_array_create(L, condition_all_eval);
}

static wfx_condition_rc_t condition_match_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  wfx_data_t     *data = &self->data;
  ngx_str_t       str[32];
  
  ngx_str_t       scur;
  ngx_str_t       ccur;
  
  wfx_str_part_t *parts;
  wfx_str_t     **wstrs = data->data.str_array;
  int             i, smax, cmax, si, ci;
  
  int             negate = self->negate;
  int             cmp;

  
  if(data->count == 0)
    return 0;
  else if(data->count == 1) //1 string always matches
    return 1; 
  
  smax = wstrs[0]->parts_count;
  parts = wstrs[0]->parts;
  
  for(si=0; si < smax; si++) {
    //build first (simplest) string
    wfx_str_get_part_value(wstrs[0], &parts[si], &str[si], ed); 
  }
  
  //there's room for optimization here, guys. just not prematurely.
  
  for(i=1; i < data->count; i++) {
    //compare piecemeal against others
    si = 0;
    scur = str[si];

    ci = 0;
    cmax = wstrs[i]->parts_count;
    parts = wstrs[i]->parts;
    wfx_str_get_part_value(wstrs[i], &parts[ci], &ccur, ed); 
    
    while(1) {
      if(scur.len < ccur.len) {
        cmp = memcmp(scur.data, ccur.data, scur.len);
        ccur.data+=scur.len;
        ccur.len -=scur.len;
        
        if(cmp == 0 && !negate)
          return 0;
        if(cmp != 0 && negate)
          return 1;
        
        if(++si < smax)
          scur = str[si];
        else
          break;
      }
      else {
        cmp = memcmp(scur.data, ccur.data, ccur.len);
        scur.data+=ccur.len;
        scur.len -=ccur.len;
        
        if(cmp == 0 && !negate)
          return 0;
        if(cmp != 0 && negate)
          return 1;
        
        if(++ci < cmax)
          wfx_str_get_part_value(wstrs[i], &parts[ci], &ccur, ed); 
        else
          break;
      }
    }
  }
  
  return !negate;
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

static wfx_condition_type_t condition_types[] = {
  {"true", condition_true_create, NULL},
  {"false", condition_false_create, NULL},
  {"any", condition_any_create, NULL},
  {"match", condition_match_create, NULL},
  {"all", condition_all_create, NULL},
  {NULL, NULL, NULL}
};
