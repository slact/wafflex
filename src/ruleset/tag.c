#include <ngx_wafflex.h>
#include "common.h"
#include "tag.h"
#include "condition.h"
#include "action.h"
#include "tracer.h"
#include <util/wfx_str.h>

int push_tag_chunks(lua_State *L, wfx_data_t *d, wfx_evaldata_t *ed) {
  wfx_str_t      *wstr = d->data.str;
  wfx_str_part_t *parts = wstr->parts;
  ngx_str_t       str;
  int             i, n = wstr->parts_count;
  for(i=0; i < n; i++) {
    //build first (simplest) string
    wfx_str_get_part_value(wstr, &parts[i], &str, ed); 
    lua_pushlstring(L, (const char *)str.data, str.len);
  }
  return n;
}

//tag-check
static wfx_condition_rc_t condition_tag_check_eval(wfx_condition_t *self, wfx_evaldata_t *ed, wfx_condition_stack_t *stack) {
  int             found;
  u_char          key_sha1[20];
  
  wfx_lua_getfunction(wfx_Lua, "findTag");
  switch(ed->type) {
    case WFX_EVAL_HTTP_REQUEST: 
      lua_pushlightuserdata(wfx_Lua, ed->data.request);
      break;
    default:
      ERR("don't know how to do that");
      return WFX_COND_ERROR;
  }
  wfx_str_sha1(self->data.data.str, ed, key_sha1);
  tracer_log_wstr(ed, "string", self->data.data.str);
  lua_pushlstring(wfx_Lua, (const char *)key_sha1, 20);
  lua_call(wfx_Lua, 2, 1);
  found = lua_toboolean(wfx_Lua, -1);
  lua_pop(wfx_Lua, 1);
  DBG("CONDITION: %s%s tag \"%V\" for request %p", self->negate ? "(negate) " : "", found ? "found" : "notfound", wfx_str_as_dbg_ngx_str(self->data.data.str, ed), ed->data.request);
  
  
  return found && !self->negate ? WFX_COND_TRUE : WFX_COND_FALSE;
}
static int condition_tag_check_create(lua_State *L) {
  wfx_condition_t *condition = condition_create(L, 0, condition_tag_check_eval);
  wfx_lua_set_str_data(L, 1, &condition->data);
  return 1;
}

static wfx_condition_type_t condition_tag_check = {
  "tag-check",
  condition_tag_check_create,
  NULL
};

void tag_request_cleanup(ngx_http_request_t *r) {
  wfx_lua_getfunction(wfx_Lua, "clearTags");
  lua_pushlightuserdata(wfx_Lua, r);
  lua_call(wfx_Lua, 1, 0);
}

static wfx_rc_t action_tag_eval(wfx_action_t *self, wfx_evaldata_t *ed) { //set-tag
  u_char          key_sha1[20];
  
  wfx_lua_getfunction(wfx_Lua, "setTag");
  switch(ed->type) {
    case WFX_EVAL_HTTP_REQUEST: 
      lua_pushlightuserdata(wfx_Lua, ed->data.request);
      break;
    default:
      ERR("don't know how to do that");
      return WFX_ERROR;
  }
  wfx_str_sha1(self->data.data.str, ed, key_sha1);
  tracer_log_wstr(ed, "string", self->data.data.str);
  lua_pushlstring(wfx_Lua, (const char *)key_sha1, 20);
  lua_call(wfx_Lua, 2, 1);
  if(lua_toboolean(wfx_Lua, -1)) {
    //add cleanup
    if(ed->type == WFX_EVAL_HTTP_REQUEST) {
      ngx_http_cleanup_t   *cln;
      cln = ngx_http_cleanup_add(ed->data.request, 0);
      if(!cln) {
        tag_request_cleanup(ed->data.request);
        ERR("failed to set tag: out pf memory probably");
      }
      else {
        cln->handler = (ngx_http_cleanup_pt )tag_request_cleanup;
        cln->data = ed->data.request;
      }
    }
  }
  
  DBG("ACTION: set tag \"%V\" for request %p", wfx_str_as_dbg_ngx_str(self->data.data.str, ed), ed->data.request);
  
  lua_pop(wfx_Lua, 1);
  return WFX_OK;
}
static int action_tag_create(lua_State *L) {
  wfx_action_t *action = action_create(L, 0, action_tag_eval);
  wfx_lua_set_str_data(L, 1, &action->data);
  return 1;
}

static wfx_action_type_t action_tag = {
  "tag",
  action_tag_create,
  NULL
};

void wfx_tag_bindings_set(lua_State *L) {
  wfx_condition_binding_add(L, &condition_tag_check);
  wfx_action_binding_add(L, &action_tag);
}

int wfx_tag_init_runtime(lua_State *L, int manager) {
  if(!manager) {
    wfx_lua_loadscript(L, tag);
  }
  return 1;
}
