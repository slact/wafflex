#ifndef NGX_WAFFLEX_TYPES_H
#define NGX_WAFFLEX_TYPES_H
#include <lua.h>
#include <util/reaper.h>

typedef struct {
  const char           *name;
  lua_CFunction         create;
  lua_CFunction         update;
  lua_CFunction         delete;
} wfx_binding_t;

typedef struct {
  uint16_t       start;
  uint16_t       end;
  ngx_uint_t     key;
  unsigned       variable:1;
  unsigned       regex_capture:1;
} wfx_str_part_t;

typedef struct {
  ngx_str_t        str;
  int              luaref;
  uint16_t         parts_count;
  wfx_str_part_t  *parts;
} wfx_str_t;

typedef enum {WFX_COND_FALSE=0, WFX_COND_TRUE, WFX_COND_DEFER, WFX_COND_ERROR} wfx_condition_rc_t;
typedef enum {WFX_REJECT=0, WFX_ACCEPT, WFX_OK, WFX_SKIP, WFX_DEFER, WFX_ERROR} wfx_rc_t;

typedef enum {WFX_EVAL_NONE=0, WFX_EVAL_ACCEPT, WFX_EVAL_HTTP_REQUEST} wfx_evaldata_type_t;
typedef enum {WFX_PHASE_CONNECT, WFX_PHASE_HTTP_REQUEST_HEADERS, WFX_PHASE_HTTP_REQUEST_BODY, WFX_PHASE_HTTP_RESPOND_HEADERS, WFX_PHASE_HTTP_RESPOND_BODY, WFX_PHASE_INVALID} wfx_phase_type_t;

typedef struct {
  ngx_atomic_uint_t reading;
  ngx_atomic_uint_t writing;
} wfx_readwrite_t;

typedef struct {
  int           luaref;
  unsigned      on:1;
} wfx_tracer_t;

typedef struct wfx_condition_s wfx_condition_t;

typedef struct {
  ngx_atomic_int_t uses;
  wfx_condition_t *condition;
  unsigned         profile;
  unsigned         trace;
} wfx_tracer_round_t;

typedef struct {
  wfx_evaldata_type_t  type;
  wfx_phase_type_t     phase;
  wfx_tracer_t         tracer;
  union {
    ngx_http_request_t   *request;
  }                     data;
} wfx_evaldata_t;

typedef enum {WFX_UNKNOWN_ELEMENT = 0, WFX_RULESET, WFX_PHASE, WFX_LIST, WFX_RULE, WFX_CONDITION, WFX_ACTION} wfx_ruleset_element_type_t;

typedef enum {WFX_DATA_INTEGER = 0, WFX_DATA_FLOAT, WFX_DATA_STRING, WFX_DATA_STRING_ARRAY, WFX_DATA_PTR} wfx_data_type_t;
typedef struct {
  wfx_data_type_t   type;
  int               count;
  union {
    void              *ptr;
    wfx_str_t         *str;
    wfx_str_t         *str_array[1];
    ngx_int_t          integer;
    float              floating_point;
  }                 data;
} wfx_data_t;

typedef struct {
  ngx_atomic_uint_t  shmem_pages_used;
  wfx_tracer_round_t tracer_rounds[16];
  ngx_atomic_uint_t  tracer_rounds_count;
} wfx_shm_data_t;

typedef struct wfx_limiter_value_s wfx_limiter_value_t;
struct wfx_limiter_value_s {
  u_char            key[20];
  ngx_atomic_int_t  count;
  time_t            time;
  wfx_limiter_value_t *prev;
  wfx_limiter_value_t *next;

  int               refcount; //handled strictly by the manager
}; //wfx_limiter_value_t

typedef struct wfx_limiter_s wfx_limiter_t;
struct wfx_limiter_s {
  char             *name;
  int               luaref;
  wfx_readwrite_t   rw;
  time_t            interval;
  ngx_int_t         limit;
  ngx_int_t         sync_steps;
  reaper_t         *values;
  struct {
    wfx_limiter_t    *limiter;
    time_t            expire;
  }                 burst;
}; //wfx_limiter_t

typedef struct wfx_rule_s wfx_rule_t;

typedef struct wfx_condition_stack_el_s wfx_condition_stack_el_t;
struct wfx_condition_stack_el_s {
  void                     *pd;
  wfx_condition_stack_el_t *next;
}; //wfx_condition_stack_el_t

typedef struct {
  wfx_condition_stack_el_t *head;
  wfx_condition_stack_el_t *tail;
} wfx_condition_stack_t;

typedef wfx_condition_rc_t (*wfx_condition_eval_pt)(wfx_condition_t *, wfx_evaldata_t *, wfx_condition_stack_t *stack);
struct wfx_condition_s {
  char             *condition;
  int               luaref;
  wfx_condition_eval_pt eval;
  unsigned          negate:1;
  
  wfx_data_t        data;
}; //wfx_condition_t

typedef struct wfx_action_s wfx_action_t;
typedef wfx_rc_t (*wfx_action_eval_pt)(wfx_action_t *, wfx_evaldata_t *);
struct wfx_action_s {
  char             *action;
  int               luaref;
  wfx_action_eval_pt eval;
  wfx_data_t        data;
}; //wfx_action_t

struct wfx_rule_s {
  char             *name;
  int               luaref;
  int               gen;
  wfx_readwrite_t   rw;
  wfx_condition_t  *condition;
  size_t            then_actions_count;
  size_t            else_actions_count;
  wfx_action_t     *(*all_actions);
}; //wfx_rule_t

typedef struct {
  char             *name;
  int               luaref;
  int               gen;
  wfx_readwrite_t   rw;
  size_t            len;
  wfx_rule_t       *rules[1];
} wfx_rule_list_t;

typedef struct {
  char             *name;
  int               luaref;
  int               gen;
  wfx_readwrite_t   rw;
  size_t            len;
  wfx_rule_list_t  *lists[1];
} wfx_phase_t;

typedef struct {
  char             *name;
  int               luaref;
  int               gen;
  wfx_readwrite_t   rw;
  wfx_phase_t      *phase[WFX_PHASE_INVALID];
} wfx_ruleset_t;


//redis stuff
typedef struct {
  ngx_str_t                     url;
  ngx_flag_t                    url_enabled;
  time_t                        ping_interval;
  ngx_str_t                     namespace;
  ngx_str_t                     upstream_url;
  ngx_http_upstream_srv_conf_t *upstream;
  ngx_flag_t                    upstream_inheritable;
  unsigned                      enabled:1;
  void                         *privdata;
} wfx_redis_conf_t;

//config stuff
typedef struct {
  ngx_str_t           name;
  wfx_ruleset_t      *ruleset; //points to shared memory
} wfx_ruleset_conf_t;

typedef struct {
  size_t              shm_size;
  unsigned            enabled:1;
} wfx_main_conf_t;

typedef struct {
  ngx_array_t        *rulesets;
} wfx_srv_conf_t;

typedef struct {
  ngx_array_t        *rulesets;
  wfx_redis_conf_t    redis;
} wfx_loc_conf_t;

//request context
typedef struct {
  struct {
    int                 i;
    int                 gen; //current ruleset generation
  }                   ruleset;
  struct {
    int                 gen;
    wfx_phase_type_t    phase;
  }                   phase;
  struct {
    int                 i;
    int                 gen;
  }                   list;
  struct {
    int                 i;
    int                 gen;
    wfx_condition_stack_t condition_stack;
    unsigned              condition_done:1;
    unsigned              condition_true:1;
    struct {
      int                 i;
      void               *data;
    }                   action;
  }                   rule;
  struct {
    ngx_http_cleanup_t *cln;
  }                   limiter;
  unsigned            nocheck:1;
  
} wfx_request_ctx_t;

#endif //NGX_WAFFLEX_TYPES_H
