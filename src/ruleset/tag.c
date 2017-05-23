#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "tag.h"
#include "condition.h"




//tag-check
static int condition_tag_check_eval(wfx_condition_t *self, wfx_rule_t *rule, ngx_connection_t *c, ngx_http_request_t *r) {
  return self->negate ? 1 : 0;
}
static int condition_tag_check_create(lua_State *L) {
  wfx_condition_t *condition = condition_create(L, 0, condition_tag_check_eval);
  condition->data = condition->data;
  return 1;
}
static wfx_condition_type_t condition_tag_check = {
  "tag-check",
  condition_tag_check_create,
  NULL
};

void wfx_tag_bindings_set(lua_State *L) {
  wfx_condition_binding_add(L, &condition_tag_check);
}
