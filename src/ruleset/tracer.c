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
#include <assert.h>

#define TRACER_OR_BUST(ed, t) \
  if(!ed->tracer.on) return;  \
  wfx_tracer_t       *t = &ed->tracer
  
#define TRACER_ROUND_CLEANUP_INTERVAL_MSEC 1000
  
static void tracer_lua_call(lua_State *L, wfx_tracer_t *t, const char *func, int nargs, int nresp) {
  wfx_lua_pushfromref(L, t->luaref);
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
  if(wfx_shm_data->tracer_rounds_count == 0) {
    ed->tracer.on = 0;
    ed->tracer.luaref = LUA_NOREF;
    return;
  }
  int                    i;
  wfx_condition_stack_t  condition_stack;
  wfx_condition_rc_t     cond_rc = WFX_COND_FALSE;
  wfx_tracer_round_t    *tracer_round = NULL;
  
  ngx_memzero(&condition_stack, sizeof(condition_stack));
  
  for(i=0; i<16; i++) {
    tracer_round = &wfx_shm_data->tracer_rounds[i];
    if(tracer_round->uses <= 0) {
      continue;
    }
    cond_rc = tracer_round->condition->eval(tracer_round->condition, ed, &condition_stack);
    switch (cond_rc) {
      case WFX_COND_DEFER:
        ERR("ignoring deferrable condition in tracer round");
        condition_stack_clear(&condition_stack);
        continue;
      case WFX_COND_FALSE:
      case WFX_COND_ERROR:
        condition_stack_clear(&condition_stack);
        continue;
      case WFX_COND_TRUE:
        condition_stack_clear(&condition_stack);
        break;
    }
  }
  
  if(cond_rc != WFX_COND_TRUE) {
    ed->tracer.on = 0;
    ed->tracer.luaref = LUA_NOREF;
    return;
  }
  else {
    ngx_atomic_fetch_add((ngx_atomic_uint_t *)&tracer_round->uses, -1);
    ed->tracer.on = 1;
    wfx_ipc_alert_cachemanager("tracer-round-fired", tracer_round, sizeof(tracer_round));
  }
  
  TRACER_OR_BUST(ed, t);
  if(t->luaref == LUA_NOREF) {
    wfx_lua_getlib_field(wfx_Lua, "Tracer", "get");
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
    t->luaref = wfx_lua_ref(wfx_Lua, -1);
  }
}
void tracer_finish(wfx_evaldata_t *ed) {
  TRACER_OR_BUST(ed, t);
  tracer_lua_call(wfx_Lua, t, "finish", 0, 0);
}

int tracer_round_cleanup(void *data) {
  int                     i, remaining = 0;
  wfx_tracer_round_t     *round;
  for(i=0; i<16; i++) {
    round = &wfx_shm_data->tracer_rounds[i];
    if(round->uses == 0) {
      wfx_lua_getlib_field(wfx_Lua, "Tracer", "delete");
      wfx_lua_pushfromref(wfx_Lua, round->luaref);
      lua_ngxcall(wfx_Lua, 1, 0);
    }
    else {
      remaining++;
    }
  }
  
  return remaining == 0 ? 0 : TRACER_ROUND_CLEANUP_INTERVAL_MSEC;
}

static int tracer_round_create(lua_State *L) {
  ERR("tracer round create");
  wfx_tracer_round_t     *round;
  int                     i, found = 0;
  const char             *str;
  
  for(i=0; i<16; i++) {
    round = &wfx_shm_data->tracer_rounds[i];
    if(round->uses == 0) {
      found = 1;
      break;
    } 
  }
  if(!found) {
    ERR("can't create tracer round, all 16 rounds in use");
    return 0;
  }
  
  if(round->condition) { //remove previous condition
    //TODO
    assert(0);
  }
  
  round->luaref = wfx_lua_ref(L, -1);
  
  lua_getfield(L, -1, "condition");
  lua_getfield(L, -1, "__binding");
  round->condition=lua_touserdata(L, -1);
  lua_pop(L, 2);
  
  lua_getfield(L, -1, "profile");
  round->profile = lua_toboolean(L, -1);
  lua_pop(L, 1);
  
  lua_getfield(L, -1, "trace");
  round->profile = lua_toboolean(L, -1);
  lua_pop(L, 1);
  
  lua_getfield(L, -1, "target");
  str = lua_tostring(L, -1);
  if (strcmp("redis", str) == 0) {
    round->target = WFX_TRACER_TARGET_REDIS;
  }
  else {
    round->target = WFX_TRACER_TARGET_LOG;
  }
  lua_pop(L, 1);
  
  if(round->target == WFX_TRACER_TARGET_REDIS) {
    lua_getfield(L, -1, "target_type");
    str = lua_tostring(L, -1);
    if (strcmp("pubsub", str) == 0) {
      round->target = WFX_TRACER_TARGET_REDIS_PUBSUB;
    }
    else {
      round->target = WFX_TRACER_TARGET_REDIS_KEY;
    }
    lua_pop(L, 1);
    
    lua_getfield(L, -1, "target_key");
    lua_tongxstr(L, -1, &round->redis_target_key);
    lua_pop(L, 1);
  }
  
  lua_getfield(L, -1, "uses");
  round->uses = lua_tonumber(L, -1);
  lua_pop(L,1);
  
  //init tracer cleanup timer
  wfx_add_reusable_timer(tracer_round_cleanup, 0, TRACER_ROUND_CLEANUP_INTERVAL_MSEC);
  
  lua_pushlightuserdata (L, round);
  return 1;
}

static int tracer_round_delete(lua_State *L) {
  wfx_tracer_round_t   *round = lua_touserdata(L, 1);
  wfx_lua_unref(L, round->luaref);
  round->condition = NULL;
  return 0;
}

static wfx_binding_t wfx_tracer_round_binding = {
  "tracer-round",
  tracer_round_create,
  NULL,
  tracer_round_delete
};

int wfx_tracer_init_runtime(lua_State *L, int manager) {
  wfx_lua_loadscript(L, tracer);
  wfx_lua_binding_set(L, &wfx_tracer_round_binding);
  return 1;
}

int tracer_round_load(lua_State *L) {
  //for(i=0; i<16; i++) {
  //  tracer_round = &wfx_shm_data->tracer_rounds[i];
  //end

  return 0;
}
