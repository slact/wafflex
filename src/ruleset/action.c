#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "action.h"

static wfx_action_type_t action_types[];

wfx_action_t *action_create(lua_State *L, size_t data_sz, wfx_action_eval_pt eval) {
  wfx_action_t *action = ruleset_common_shm_alloc_init_item(wfx_action_t, data_sz, L, action);
  action->eval = eval;
  action->data = data_sz == 0 ? NULL : &action[1];
  lua_pushlightuserdata(L, action);
  return action;
}

#define action_to_binding(action, binding, buf) \
  snprintf(buf, 255, "action:%s", action->name); \
  (binding).name = buf; \
  (binding).create = action->create; \
  (binding).update = NULL; \
  (binding).replace = NULL; \
  (binding).delete = action->destroy

void wfx_action_bindings_set(lua_State *L) {;
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
static wfx_action_result_t action_accept_eval(wfx_action_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  //TODO
  return WFX_ACTION_FINISH;
}
static int action_accept_create(lua_State *L) {
  wfx_action_t *action = action_create(L, 0, action_accept_eval);
  action->data = action->data;
  //TODO
  return 1;
}


//reject
static wfx_action_result_t action_reject_eval(wfx_action_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  //TODO
  return WFX_ACTION_FINISH;
}
static int action_reject_create(lua_State *L) {
  wfx_action_t *action = action_create(L, 0, action_reject_eval);
  action->data = action->data;
  //TODO
  return 1;
}

static wfx_action_result_t action_tag_eval(wfx_action_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  //TODO
  return WFX_ACTION_NEXT;
}
static int action_tag_create(lua_State *L) {
  wfx_action_t *action = action_create(L, 0, action_tag_eval);
  action->data = action->data;
  //TODO
  return 1;
}


static wfx_action_type_t action_types[] = {
  {"accept", action_accept_create, NULL},
  {"reject", action_reject_create, NULL},
  {"tag", action_tag_create, NULL},
  {NULL, NULL, NULL}
};
