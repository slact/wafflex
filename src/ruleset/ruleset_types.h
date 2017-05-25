#ifndef WFX_RULESET_TYPES_H
#define WFX_RULESET_TYPES_H

#include <nginx.h>
#include <ngx_http.h>

typedef enum{WFX_ACTION_NEXT, WFX_ACTION_BREAK, WFX_ACTION_FINISH} wfx_action_result_t;

typedef struct wfx_limiter_s wfx_limiter_t;
struct wfx_limiter_s {
  char             *name;
  int               luaref;
  time_t            interval;
  ngx_int_t         limit;
  ngx_int_t         sync_steps;
  struct {
    wfx_limiter_t    *limiter;
    time_t            expire;
  }                 burst;
}; //wfx_limiter_t


typedef struct wfx_rule_s wfx_rule_t;

typedef struct wfx_condition_s wfx_condition_t;
typedef int (*wfx_condition_eval_pt)(wfx_condition_t *, wfx_rule_t *, ngx_connection_t *,ngx_http_request_t *);
struct wfx_condition_s {
  char             *condition;
  int               luaref;
  wfx_condition_eval_pt eval;
  unsigned          negate:1;
  
  wfx_data_t        data;
}; //wfx_condition_t

typedef struct wfx_action_s wfx_action_t;
typedef wfx_action_result_t (*wfx_action_eval_pt)(wfx_action_t *, wfx_rule_t *, ngx_connection_t *,ngx_http_request_t *);
struct wfx_action_s {
  char             *action;
  int               luaref;
  wfx_action_eval_pt eval;
  wfx_data_t        data;
}; //wfx_action_t

struct wfx_rule_s {
  char             *name;
  int               luaref;
  wfx_condition_t  *condition;
  int               actions_len;
  wfx_action_t     *(*actions);
  int               else_actions_len;
  wfx_action_t     *(*else_actions);
}; //wfx_rule_t

typedef struct {
  char             *name;
  int               luaref;
  size_t            len;
  wfx_rule_t       *rules[1];
} wfx_rule_list_t;

typedef struct {
  char             *name;
  int               luaref;
  size_t            len;
  wfx_rule_list_t  *lists[1];
} wfx_phase_t;

typedef struct {
  wfx_phase_t      *request;
  wfx_phase_t      *respond;
} wfx_phases_t;

typedef struct {
  char             *name;
  int               luaref;
  wfx_phases_t      phase;
  
} wfx_ruleset_t;


/*
typedef struct wfx_binding_chain_s wfx_binding_chain_t;
struct wfx_binding_chain_s {
  wfx_binding_t        binding;
  wfx_binding_chain_t *next;
}; //wfx_binding_chain_t

void ruleset_subbinding_push(wfx_binding_chain_t **headptr, wfx_binding_t *binding);
void ruleset_subbinding_bind(lua_State *L, const char *name, wfx_binding_chain_t **headptr);
int ruleset_subbinding_call(lua_State *L,const char *binding_name, const char *subbinding_name, const char *call_name, void *ptr, int nargs);
int ruleset_subbinding_getname_call(lua_State *L, const char *binding_name, const char *call_name, int nargs);
*/

#define ruleset_common_shm_alloc_init_item(type, data_sz, L, name_key) \
  __ruleset_common_shm_alloc_init_item(L, sizeof(type), data_sz, #name_key, offsetof(type, name_key), offsetof(type, luaref))
  
#define ruleset_common_shm_alloc_init_item_noname(type, data_sz, L) \
  __ruleset_common_shm_alloc_init_item_noname(L, sizeof(type), data_sz, offsetof(type, luaref))
  
void * __ruleset_common_shm_alloc_init_item(lua_State *L, size_t item_sz, size_t data_sz, char *str_key, off_t str_offset, off_t luaref_offset);
void * __ruleset_common_shm_alloc_init_item_noname(lua_State *L, size_t item_sz, size_t data_sz, off_t luaref_offset);

#endif //WFX_RULESET_TYPES_H
