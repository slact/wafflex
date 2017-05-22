#include <ngx_wafflex.h>
#include "ruleset_types.h"
#include "ruleset.h"
#include "list.h"
#include "rule.h"
#include "condition.h"
#include "action.h"
#include "limiter.h"
#include "phase.h"


static int ruleset_create(lua_State *L) {
  const char        *str;
  
  wfx_ruleset_t     *ruleset;
  wfx_phase_t       *phase = NULL;
  
  ruleset = ruleset_common_shm_alloc_init_item(wfx_ruleset_t, 0, L, name);
  if(ruleset == NULL) {
    ERR("failed to initialize ruleset: out of memory");
  }
  
  lua_getfield(L, -1, "phases");
  lua_pushnil(L);  // first key 
  while (lua_next(L, -2) != 0) {
    lua_getfield(L, -1, "__binding");
    phase = lua_touserdata(L, -1);
    lua_pop(L,1);
    
    str = lua_tostring(L, -2);
    
    if(strcmp(str, "request") == 0) {
      ruleset->phase.request = phase;
    }
    else {
      ERR("unknown phase %s", str);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
  
  
  lua_pushlightuserdata (L, ruleset);
  return 1;
}
static int ruleset_replace(lua_State *L) {
  ERR("ruleset replace");
  return 0;
}
static int ruleset_update(lua_State *L) {
  ERR("ruleset update");
  return 0;
}
static int ruleset_delete(lua_State *L) {
  ERR("ruleset delete");
  return 0;
}

int wfx_ruleset_bind_lua(lua_State *L) {
  //rule bindings
  wfx_lua_register(L, wfx_rule_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  //rule condition bindings
  wfx_lua_register(L, wfx_condition_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  //rule action bindings
  wfx_lua_register(L, wfx_action_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  //list bindings
  wfx_lua_register(L, wfx_list_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  //limiter bindings
  wfx_lua_register(L, wfx_limiter_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
    //limiter bindings
  wfx_lua_register(L, wfx_phase_bind_lua);
  lua_pushvalue(L, 1);
  lua_ngxcall(L, 1, 0);
  
  //and now, at last, ruleset bindings
  lua_pushstring(L, "ruleset");
  wfx_lua_register(L, ruleset_create);
  wfx_lua_register(L, ruleset_replace);
  wfx_lua_register(L, ruleset_update);
  wfx_lua_register(L, ruleset_delete);
  lua_ngxcall(L,5,0);
  return 0;
}

void * __ruleset_common_shm_alloc_init_item(lua_State *L, size_t item_sz, char *str_key, off_t str_offset, off_t luaref_offset) {
  ngx_str_t  tmpstr;
  int        tmpref;
  void      *ptr;
  char      *str;
  char     **strptr;
  int       *refptr;
  
  lua_pushvalue(L, 1); //lua-self
  tmpref = luaL_ref(L, LUA_REGISTRYINDEX);
  
  lua_pushvalue(L, 1); //lua-self
  lua_getfield(L, -1, str_key);
  luaL_checklstring_as_ngx_str(L, -1, &tmpstr);
  
  if((ptr = wfx_shm_calloc(item_sz + tmpstr.len + 1)) != NULL) {
    str = (char *)ptr + item_sz;
    strptr = (char **)((char *)ptr + str_offset);
    strcpy((char *)ptr + item_sz, (char *)tmpstr.data);
    *strptr = str;
    refptr = (int *)((char *)ptr + luaref_offset);
    *refptr = tmpref;
  }
  
  lua_pop(L, 2);
  return ptr;
}

static wfx_phase_t *ruleset_find_phase(wfx_ruleset_t *ruleset, const char *name) {
  
  return NULL;
}
