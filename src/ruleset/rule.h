#ifndef WFX_RULE_H
#define WFX_RULE_H

void wfx_rule_bindings_set(lua_State *L);
wfx_rc_t wfx_rule_eval(wfx_rule_t *, wfx_evaldata_t *);
#endif //WFX_RULE_H
