#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "condition.h"

static wfx_condition_type_t condition_types[];

wfx_condition_t *condition_create(lua_State *L, size_t extra_sz) {
  wfx_condition_t *condition = ruleset_common_shm_alloc_init_item(wfx_condition_t, extra_sz, L, condition);
  
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
static int condition_true_eval(wfx_condition_t *self, ngx_connection_t *c, ngx_http_request_t *r) {
  return self->negate ? 0 : 1;
}
static int condition_true_create(lua_State *L) {
  wfx_condition_t *condition = condition_create(L, 0);
  condition->eval = condition_true_eval;
  return 1;
}

//false
static int condition_false_eval(wfx_condition_t *self, ngx_connection_t *c, ngx_http_request_t *r) {
  return self->negate ? 1 : 0;
}
static int condition_false_create(lua_State *L) {
  wfx_condition_t *condition = condition_create(L, 0);
  condition->eval = condition_false_eval;
  return 1;
}


//any
static int condition_any_eval(wfx_condition_t *self, ngx_connection_t *c, ngx_http_request_t *r) {
  return 1;
}
static int condition_any_create(lua_State *L) {
  wfx_condition_t *condition = condition_create(L, 0);
  condition->eval = condition_any_eval;
  return 1;
}

//all
static int condition_all_eval(wfx_condition_t *self, ngx_connection_t *c, ngx_http_request_t *r) {
  return 1;
}
static int condition_all_create(lua_State *L) {
  wfx_condition_t *condition = condition_create(L, 0);
  condition->eval = condition_all_eval;
  return 1;
}

//tag-check
static int condition_tagcheck_eval(wfx_condition_t *self, ngx_connection_t *c, ngx_http_request_t *r) {
  return self->negate ? 1 : 0;
}
static int condition_tagcheck_create(lua_State *L) {
  wfx_condition_t *condition = condition_create(L, 0);
  condition->eval = condition_tagcheck_eval;
  return 1;
}

static wfx_condition_type_t condition_types[] = {
  {"true", condition_true_create, NULL},
  {"false", condition_false_create, NULL},
  {"any", condition_any_create, NULL},
  {"all", condition_all_create, NULL},
  
  {"tag-check", condition_tagcheck_create, NULL},
  {NULL, NULL, NULL}
};
