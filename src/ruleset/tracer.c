#include <ngx_wafflex.h>
#include "common.h"
#include "ruleset.h"
#include "list.h"
#include "rule.h"
#include "condition.h"
#include "action.h"
#include "tag.h"
#include "limiter.h"
#include "phase.h"
#include "string.h"
#include "tracer.h"
#include <util/wfx_str.h>

#define TRACER_OR_BUST(ed, t) \
  if(!ed->tracer.on) return;  \
  wfx_tracer_t       *t = &ed->tracer
  
static int ngx_lua_msec_cached_time(lua_State *L) {
  ngx_time_t *t = ngx_timeofday();
  lua_pushnumber(L, t->sec);
  lua_pushnumber(L, t->msec);
  return 2;
}

static int ngx_lua_sec_cached_time(lua_State *L) {
  lua_pushnumber(L, ngx_time());
  return 1;
}
  
static void tracer_lua_call(lua_State *L, wfx_tracer_t *t, const char *func, int nargs, int nresp) {
  wfx_lua_fromref(L, t->luaref);
  lua_getfield(L, -1, func);
  
  lua_insert(L, -(nargs + 2));
  lua_insert(L, -(nargs + 1));
  
  lua_ngxcall(L, nargs + 1, 0);
}

static const char *element_type_cstr(wfx_ruleset_element_type_t et) {
  switch(et) {
    case WFX_UNKNOWN_ELEMENT:
      return "<?>";
    case WFX_RULESET:
      return "ruleset";
    case WFX_PHASE:
      return "phase";
    case WFX_LIST:
      return "list";
    case WFX_RULE:
      return "rule";
    case WFX_CONDITION:
      return "condition";
    case WFX_ACTION:
      return "action";
    default:
      break;
  }
  return "";
}

static const char *evaldata_type(wfx_evaldata_t *ed) {
  switch(ed->type) {
    case WFX_EVAL_NONE:
      return "none";
    case WFX_EVAL_ACCEPT:
      return "accept";
    case WFX_EVAL_HTTP_REQUEST:
      return "request";
  }
  return "?";
}

static const char *element_name_cstr(wfx_ruleset_element_type_t et, void *el) {
  switch(et) {
    case WFX_UNKNOWN_ELEMENT:
      return "<?>";
    case WFX_RULESET:
      return ((wfx_ruleset_t *)el)->name;
    case WFX_PHASE:
      return ((wfx_phase_t *)el)->name;
    case WFX_LIST:
      return ((wfx_rule_list_t *)el)->name;
    case WFX_RULE:
      return ((wfx_rule_t *)el)->name;
    case WFX_CONDITION:
      return ((wfx_condition_t *)el)->condition;
    case WFX_ACTION:
      return ((wfx_action_t *)el)->action;
    default:
      break;
  }
  return "<...>";
}

/*
static int element_luaref(wfx_ruleset_element_type_t et, void *el) {
  switch(et) {
    case WFX_RULESET:
      return ((wfx_ruleset_t *)el)->luaref;
    case WFX_PHASE:
      return ((wfx_phase_t *)el)->luaref;
    case WFX_LIST:
      return ((wfx_rule_list_t *)el)->luaref;
    case WFX_RULE:
      return ((wfx_rule_t *)el)->luaref;
    case WFX_UNKNOWN_ELEMENT:
    default:
      break;
  }
  return 0;
}
*/

static int element_gen(wfx_ruleset_element_type_t et, void *el) {
  switch(et) {
    case WFX_RULESET:
      return ((wfx_ruleset_t *)el)->gen;
    case WFX_PHASE:
      return ((wfx_phase_t *)el)->gen;
    case WFX_LIST:
      return ((wfx_rule_list_t *)el)->gen;
    case WFX_RULE:
      return ((wfx_rule_t *)el)->gen;
    case WFX_UNKNOWN_ELEMENT:
    default:
      break;
  }
  return 0;
}

static const char* element_rc_str(wfx_ruleset_element_type_t type, int rawrc) {
  wfx_rc_t rc = rawrc;
  wfx_condition_rc_t crc = rawrc;
  
  switch(type) {
    case WFX_RULESET:
    case WFX_PHASE:
    case WFX_LIST:
    case WFX_RULE:
    case WFX_ACTION:
    case WFX_UNKNOWN_ELEMENT:
      switch(rc) {
        case WFX_REJECT:
          return "reject";
        case WFX_ACCEPT:
          return "accept";
        case WFX_OK:
          return "ok";
        case WFX_SKIP:
          return "skip";
        case WFX_DEFER:
          return "defer";
        case WFX_ERROR:
          return "error";
      }
      return "?";
    case WFX_CONDITION:
      switch(crc) {
        case WFX_COND_FALSE:
          return "false";
        case WFX_COND_TRUE:
          return "true";
        case WFX_COND_DEFER:
          return "defer";
        case WFX_COND_ERROR:
          return "error";
      }
      return "?";
  }
  return "?";
}

void tracer_push(wfx_evaldata_t *ed, wfx_ruleset_element_type_t type, void *el) {
  TRACER_OR_BUST(ed, t);
  lua_pushstring(wfx_Lua, element_type_cstr(type));
  lua_pushstring(wfx_Lua, element_name_cstr(type, el));
  lua_pushnumber(wfx_Lua, element_gen(type, el));
  lua_pushlightuserdata(wfx_Lua, el);
  tracer_lua_call(wfx_Lua, t, "push", 4, 0);
}
void tracer_pop(wfx_evaldata_t *ed, wfx_ruleset_element_type_t type, int rc) {
  TRACER_OR_BUST(ed, t);
  lua_pushstring(wfx_Lua, element_type_cstr(type));
  lua_pushstring(wfx_Lua, element_rc_str(type, rc));
  tracer_lua_call(wfx_Lua, t, "pop", 2, 0);
}
void tracer_unwind(wfx_evaldata_t *ed, wfx_ruleset_element_type_t type, const char *reason) {
  TRACER_OR_BUST(ed, t);
  lua_pushstring(wfx_Lua, element_type_cstr(type));
  lua_pushstring(wfx_Lua, reason);
  tracer_lua_call(wfx_Lua, t, "unwind", 2, 0);
}
void tracer_log_wstr(wfx_evaldata_t *ed, const char *name, wfx_str_t *wstr) {
  ngx_str_t     *str;
  TRACER_OR_BUST(ed, t);
  lua_pushstring(wfx_Lua, name);
  str = wfx_str_as_dbg_ngx_str(wstr, ed);
  lua_pushngxstr(wfx_Lua, str);
  tracer_lua_call(wfx_Lua, t, "log", 2, 0);
}
void tracer_log_wstr_array(wfx_evaldata_t *ed, const char *name, wfx_str_t *wstr) {
  TRACER_OR_BUST(ed, t);
    lua_pushstring(wfx_Lua, name);
  lua_pushngxstr(wfx_Lua, wfx_str_as_dbg_ngx_str(wstr, ed));
  tracer_lua_call(wfx_Lua, t, "log_array", 2, 0);
}
void tracer_log_str(wfx_evaldata_t *ed, const char *name, ngx_str_t *str) {
  TRACER_OR_BUST(ed, t);
  lua_pushstring(wfx_Lua, name);
  lua_pushngxstr(wfx_Lua, str);
  tracer_lua_call(wfx_Lua, t, "log", 2, 0);
}
void tracer_log_cstr(wfx_evaldata_t *ed, const char *name, char *cstr) {
  TRACER_OR_BUST(ed, t);
  lua_pushstring(wfx_Lua, name);
  lua_pushstring(wfx_Lua, cstr);
  tracer_lua_call(wfx_Lua, t, "log", 2, 0);
}
void tracer_log_number(wfx_evaldata_t *ed, const char *name, float n) {
  TRACER_OR_BUST(ed, t);
  lua_pushstring(wfx_Lua, name);
  lua_pushnumber(wfx_Lua, n);
  tracer_lua_call(wfx_Lua, t, "log", 2, 0);
}

void tracer_init(wfx_evaldata_t *ed) {
  TRACER_OR_BUST(ed, t);
  if(t->luaref == LUA_NOREF) {
    wfx_lua_getfunction(wfx_Lua, "getTracer");
    lua_pushstring(wfx_Lua, evaldata_type(ed));
    switch(ed->type) {
      case WFX_EVAL_HTTP_REQUEST:
        lua_pushlightuserdata(wfx_Lua, ed->data.request);
        break;
      default:
        raise(SIGABRT); //what do?
        break;
    }
    lua_ngxcall(wfx_Lua, 2, 1);
    t->luaref = wfx_lua_getref(wfx_Lua, -1);
  }
}
void tracer_finish(wfx_evaldata_t *ed) {
  TRACER_OR_BUST(ed, t);
  tracer_lua_call(wfx_Lua, t, "finish", 0, 0);
}

static void request_cleanup_handler(void *pd) {
  ngx_http_request_t *r = pd;
  lua_rawgetp(wfx_Lua, LUA_REGISTRYINDEX, r);
  lua_pushlightuserdata(wfx_Lua, r);
  lua_ngxcall(wfx_Lua, 1, 0);
  
  lua_pushnil(wfx_Lua);
  lua_rawsetp(wfx_Lua, LUA_REGISTRYINDEX, r);
}

static int ngx_lua_add_request_cleanup(lua_State *L) {
  ngx_http_cleanup_t   *cln;
  ngx_http_request_t   *r = lua_touserdata(L, 1);
  assert(lua_isfunction(L, 2));
  cln = ngx_http_cleanup_add(r, 0);
  if(!cln)
    return 0;
  cln->handler = request_cleanup_handler;
  cln->data = r;
  
  lua_pushvalue(L, 2);
  lua_rawsetp(L, LUA_REGISTRYINDEX, r);
  
  lua_pushboolean(L, 1);
  return 1;
}

int wfx_tracer_init_runtime(lua_State *L, int manager) {
  wfx_lua_loadscript(L, tracer);
  wfx_lua_register(L, ngx_lua_msec_cached_time);
  wfx_lua_register(L, ngx_lua_sec_cached_time);
  wfx_lua_register(L, ngx_lua_add_request_cleanup);
  lua_ngxcall(L, 3, 0);
  return 1;
}
