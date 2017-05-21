#ifndef NGX_WAFFLEX_H
#define NGX_WAFFLEX_H

#include <ngx_http.h>
#include <nginx.h>

typedef enum{STRING, ARRAY, BANANA} wfx_data_type_t;

typedef struct {
  wfx_data_type_t   type;
  union {
    void             *ptr;
    ngx_str_t         str;
    float             num;
    char              byte;
  }                 data;
} wfx_data_t;

typedef struct {
  ngx_str_t         name;
  time_t            interval;
  ngx_int_t         limit;
  ngx_int_t         sync_steps;
  struct {
    ngx_str_t         name;
    time_t            burst_expire;
    unsigned          enabled:1;
  }                 burst;
} wfx_limiter_t;

typedef struct {
  int16_t           condition;
  unsigned          negate:1;
  wfx_data_t        data;
} wfx_condition_t;

typedef struct wfx_action_s wfx_action_t;
struct wfx_action_s {
  int               action;
  wfx_action_t     *next;
  wfx_data_t        data;
}; //wfx_action_t

typedef struct {
  ngx_str_t           name;
  wfx_condition_t    *condition;
  wfx_action_t       *action;
  wfx_action_t       *else_action;
} wfx_rule_t;

typedef struct {
  ngx_str_t           name;
  size_t              len;
  wfx_rule_t         *rules;
} wfx_rule_list_t;

typedef struct {
  ngx_str_t           name;
  size_t              len;
  wfx_rule_list_t    *lists;
} wfx_phase_t;

typedef struct {
  ngx_str_t           name;
  struct {
    size_t              len;
    wfx_limiter_t      *arr;
  }                   limiters;
  struct {
    size_t              len;
    wfx_rule_list_t    *arr;
  }                   lists;
  struct {
    size_t              len;
    wfx_rule_t         *arr;
  }                   rules;
  struct {
    wfx_phase_t         connect;
    wfx_phase_t         request;
    wfx_phase_t         respond;
  }                   phases;
} wfx_ruleset_t;


extern ngx_module_t ngx_wafflex_module;

#endif //NGX_WAFFLEX_H
