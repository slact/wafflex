#ifndef WFX_CONDITION_H
#define WFX_CONDITION_H

void wfx_condition_bindings_set(lua_State *L);
wfx_condition_t *condition_create(lua_State *L, size_t data_sz, wfx_condition_eval_pt eval);

typedef struct {
  const char    *name;
  lua_CFunction  create;
  lua_CFunction  destroy;
} wfx_condition_type_t;

void wfx_condition_binding_add(lua_State *L, wfx_condition_type_t *conditiontype);

#define condition_stack_empty(stack) (stack->head == NULL)
void *condition_stack_pop(wfx_condition_stack_t *stack);
int condition_stack_push(wfx_condition_stack_t *stack, void *pd);
void condition_stack_clear(wfx_condition_stack_t *stack);

int condition_simple_destroy(lua_State *L);

void condition_stack_set_tail_data(wfx_evaldata_t *ed, void *d);
wfx_condition_rc_t condition_rc_maybe_negate(wfx_condition_t *self, wfx_condition_rc_t rc);
#endif //WFX_CONDITION_H
