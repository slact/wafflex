#ifndef WFX_TRACER_H
#define WFX_TRACER_H

void wfx_tracer_push(wfx_evaldata_t *ed, wfx_ruleset_element_type_t type, void *el);
void wfx_tracer_pop(wfx_evaldata_t *ed, wfx_ruleset_element_type_t type, int rc);
void wfx_tracer_unwind(wfx_evaldata_t *ed, wfx_ruleset_element_type_t type, const char *reason);

void wfx_tracer_log_wstr(wfx_evaldata_t *ed, const char *name, wfx_str_t *wstr);
void wfx_tracer_log_wstr_array(wfx_evaldata_t *ed, const char *name, wfx_str_t *wstr);
void wfx_tracer_log_str(wfx_evaldata_t *ed, const char *name, ngx_str_t *str);
void wfx_tracer_log_cstr(wfx_evaldata_t *ed, const char *name, char *cstr);
void wfx_tracer_log_int(wfx_evaldata_t *ed, const char *name, int n);
void wfx_tracer_log_float(wfx_evaldata_t *ed, const char *name, float f);

#endif //WFX_TRACER_H
