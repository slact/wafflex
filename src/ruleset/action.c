#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "action.h"

static wfx_action_type_t action_types[];

wfx_action_t *action_create(lua_State *L, size_t extra_sz) {
  wfx_action_t *action = ruleset_common_shm_alloc_init_item(wfx_action_t, extra_sz, L, action);
  
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
  return WFX_ACTION_NEXT;
}
static int action_accept_create(lua_State *L) {
  wfx_action_t *action = action_create(L, 0);
  action->eval = action_accept_eval;
  //TODO
  return 1;
}


//reject
static wfx_action_result_t action_reject_eval(wfx_action_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  return WFX_ACTION_NEXT;
}
static int action_reject_create(lua_State *L) {
  wfx_action_t *action = action_create(L, 0);
  action->eval = action_reject_eval;
  //TODO
  return 1;
}

static wfx_action_result_t action_tag_eval(wfx_action_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  return WFX_ACTION_NEXT;
}
static int action_tag_create(lua_State *L) {
  wfx_action_t *action = action_create(L, 0);
  action->eval = action_tag_eval;
  //TODO
  return 1;
}


static wfx_action_type_t action_types[] = {
  {"accept", action_accept_create, NULL},
  {"reject", action_reject_create, NULL},
  {"tag", action_tag_create, NULL},
  {NULL, NULL, NULL}
};
