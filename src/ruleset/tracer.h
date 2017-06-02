#ifndef WFX_TRACER_H
#define WFX_TRACER_H

void tracer_init(wfx_evaldata_t *ed);
void tracer_finish(wfx_evaldata_t *ed);

void tracer_push(wfx_evaldata_t *ed, wfx_ruleset_element_type_t type, void *el);
void tracer_pop(wfx_evaldata_t *ed, wfx_ruleset_element_type_t type, int rc);
void tracer_unwind(wfx_evaldata_t *ed, wfx_ruleset_element_type_t type, const char *reason);

void tracer_log_wstr(wfx_evaldata_t *ed, const char *name, wfx_str_t *wstr);
void tracer_log_wstr_array(wfx_evaldata_t *ed, const char *name, wfx_str_t *wstr);
void tracer_log_str(wfx_evaldata_t *ed, const char *name, ngx_str_t *str);
void tracer_log_cstr(wfx_evaldata_t *ed, const char *name, char *cstr);
void tracer_log_int(wfx_evaldata_t *ed, const char *name, int n);
void tracer_log_float(wfx_evaldata_t *ed, const char *name, float f);

int wfx_tracer_init_runtime(lua_State *L, int manager);
#endif //WFX_TRACER_H
