#ifndef WFX_RULESET_H
#define WFX_RULESET_H

int wfx_ruleset_bindings_set(lua_State *L);

wfx_rc_t wfx_ruleset_eval(wfx_ruleset_t *, wfx_evaldata_t *, wfx_request_ctx_t *);

#endif //WFX_RULESET_H
