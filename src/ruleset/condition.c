#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "condition.h"

#include <assert.h>

static wfx_condition_type_t condition_types[];

wfx_condition_t *condition_create(lua_State *L, size_t data_sz, wfx_condition_eval_pt eval) {
  wfx_condition_t *condition = ruleset_common_shm_alloc_init_item(wfx_condition_t, data_sz, L, condition);
  condition->eval = eval;
  lua_pushlightuserdata(L, condition);
  return condition;
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
static int condition_true_eval(wfx_condition_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  return self->negate ? 0 : 1;
}
static int condition_true_create(lua_State *L) {
  condition_create(L, 0, condition_true_eval);
  return 1;
}

//false
static int condition_false_eval(wfx_condition_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  return self->negate ? 1 : 0;
}
static int condition_false_create(lua_State *L) {
  condition_create(L, 0, condition_false_eval);
  return 1;
}



//any and all
static int condition_array_create(lua_State *L, wfx_condition_eval_pt eval) {
  size_t                  condition_count;
  wfx_condition_t       **conditions;
  unsigned                i;
  
  lua_getfield(L, -1, "data");
  condition_count = wfx_lua_len(L, -1);
  lua_pop(L, 1);
  
  wfx_condition_t *condition = condition_create(L, (sizeof(*conditions) * condition_count), eval);
  
  condition->data.type = WFX_DATA_RAW;
  condition->data.count = condition_count;
  conditions =(wfx_condition_t **)&condition->data.data.raw;
  
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

static int condition_list_eval(wfx_data_t *data, wfx_rule_t *rule, int stop_on, ngx_connection_t *c, ngx_http_request_t *r) {
  assert(data->type == WFX_DATA_RAW);
  wfx_condition_t       **conditions = (wfx_condition_t **)data->data.raw;
  unsigned                len = data->count;
  wfx_condition_t        *cur;
  unsigned                i;
  for(i=0; i<len; i++) {
    cur = conditions[i];
    if(stop_on == cur->eval(cur, rule, c, r)) {
      return stop_on;
    }
  }
  return !stop_on;
}

//any
static int condition_any_eval(wfx_condition_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  int                     rc = condition_list_eval(&self->data, rule, 1, c, r);
  return self->negate ? !rc : rc;
}
static int condition_any_create(lua_State *L) {
  return condition_array_create(L, condition_any_eval);
}

//all
static int condition_all_eval(wfx_condition_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  int                     rc = condition_list_eval(&self->data, rule, 0, c, r);
  return self->negate ? !rc : rc;
}
static int condition_all_create(lua_State *L) {
  return condition_array_create(L, condition_all_eval);
}

static wfx_condition_type_t condition_types[] = {
  {"true", condition_true_create, NULL},
  {"false", condition_false_create, NULL},
  {"any", condition_any_create, NULL},
  {"all", condition_all_create, NULL},
  {NULL, NULL, NULL}
};
