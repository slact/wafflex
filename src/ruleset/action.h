#ifndef WFX_ACTION_H
#define WFX_ACTION_H

void wfx_action_bindings_set(lua_State *L);

typedef struct {
  const char    *name;
  lua_CFunction  create;
  lua_CFunction  destroy;
} wfx_action_type_t;

void wfx_action_binding_add(lua_State *L, wfx_action_type_t *actiontype);

wfx_action_t *action_create(lua_State *L, size_t data_sz, wfx_action_eval_pt eval);
int action_simple_destroy(lua_State *L);

#endif //WFX_ACTION_H
