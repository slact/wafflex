#ifndef WFX_CONDITION_H
#define WFX_CONDITION_H

void wfx_condition_bindings_set(lua_State *L);

typedef struct {
  const char    *name;
  lua_CFunction  create;
  lua_CFunction  destroy;
} wfx_condition_type_t;

void wfx_condition_binding_add(lua_State *L, wfx_condition_type_t *conditiontype);

#endif //WFX_CONDITION_H
