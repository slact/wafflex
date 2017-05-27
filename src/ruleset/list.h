#ifndef WFX_LIST_H
#define WFX_LIST_H

void wfx_list_bindings_set(lua_State *L);
wfx_rc_t wfx_list_eval(wfx_rule_list_t *self, wfx_evaldata_t *ed, wfx_request_ctx_t *ctx);
#endif //WFX_LIST_H
