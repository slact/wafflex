#ifndef WFX_ACTION_H
#define WFX_ACTION_H

void wfx_action_bindings_set(lua_State *L);

typedef struct {
  const char    *name;
  lua_CFunction  create;
  lua_CFunction  destroy;
} wfx_action_type_t;

void wfx_action_binding_add(lua_State *L, wfx_action_type_t *actiontype);

#endif //WFX_ACTION_H
