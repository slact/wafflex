#include <ngx_wafflex.h>
#include "common.h"
#include "tag.h"
#include "condition.h"
#include "action.h"
#include <util/wfx_str.h>

//tag-check
static wfx_condition_rc_t condition_tag_check_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  return !self->negate ? WFX_COND_TRUE : WFX_COND_FALSE;
}
static int condition_tag_check_create(lua_State *L) {
  wfx_condition_t *condition = condition_create(L, 0, condition_tag_check_eval);
  wfx_lua_set_str_data(L, 1, &condition->data);
  return 1;
}
  
static wfx_rc_t action_tag_eval(wfx_action_t *self, wfx_evaldata_t *ed) {
  //TODO
  return WFX_OK;
}
static int action_tag_create(lua_State *L) {
  wfx_action_t *action = action_create(L, 0, action_tag_eval);
  wfx_lua_set_str_data(L, 1, &action->data);
  return 1;
}
static wfx_condition_type_t condition_tag_check = {
  "tag-check",
  condition_tag_check_create,
  NULL
};

static wfx_action_type_t action_tag = {
  "tag",
  action_tag_create,
  NULL
};

void wfx_tag_bindings_set(lua_State *L) {
  wfx_condition_binding_add(L, &condition_tag_check);
  wfx_action_binding_add(L, &action_tag);
}
