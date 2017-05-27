#ifndef WFX_PHASE_H
#define WFX_PHASE_H

void wfx_phase_bindings_set(lua_State *L);
wfx_rc_t wfx_phase_eval(wfx_phase_t *, wfx_evaldata_t *ed);
#endif //WFX_PHASE_H
