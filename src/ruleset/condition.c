#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "condition.h"
#include <util/wfx_str.h>

#include <assert.h>

static wfx_condition_type_t condition_types[];

wfx_condition_t *condition_create(lua_State *L, size_t data_sz,  wfx_condition_eval_pt eval) {
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
static int condition_true_eval(wfx_condition_t *self, wfx_evaldata_t *ed) {
  return self->negate ? 0 : 1;
}
static int condition_true_create(lua_State *L) {
  condition_create(L, 0, condition_true_eval);
  return 1;
}

//false
static int condition_false_eval(wfx_condition_t *self, wfx_evaldata_t *ed) {
  return self->negate ? 1 : 0;
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

static int condition_list_eval(wfx_data_t *data, int stop_on, wfx_evaldata_t *ed) {
  assert(data->type == WFX_DATA_PTR);
  wfx_condition_t       **conditions = (wfx_condition_t **)data->data.ptr;
  unsigned                len = data->count;
  wfx_condition_t        *cur;
  unsigned                i;
  for(i=0; i<len; i++) {
    cur = conditions[i];
    if(stop_on == cur->eval(cur, ed)) {
      return stop_on;
    }
  }
  return !stop_on;
}

//any
static int condition_any_eval(wfx_condition_t *self, wfx_evaldata_t *ed) {
  int                     rc = condition_list_eval(&self->data, 1, ed);
  return self->negate ? !rc : rc;
}
static int condition_any_create(lua_State *L) {
  return condition_array_create(L, condition_any_eval);
}

//all
static int condition_all_eval(wfx_condition_t *self, wfx_evaldata_t *ed) {
  int                     rc = condition_list_eval(&self->data, 0, ed);
  return self->negate ? !rc : rc;
}
static int condition_all_create(lua_State *L) {
  return condition_array_create(L, condition_all_eval);
}

  /*wfx_data_t     *data = &self->data;
  ngx_str_t       str[32];
  wfx_str_part_t *part;
  wfx_str_t     **wstrs = data->data.str_array;
  u_char         *cur;
  int             n;

  return self->negate ? !rc : rc;
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
